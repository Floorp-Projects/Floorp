/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "stable"
};

const observers = require('./system/events');
const { Loader, validationAttributes } = require('./content/loader');
const { Worker } = require('./content/worker');
const { Registry } = require('./util/registry');
const { EventEmitter } = require('./deprecated/events');
const { on, emit } = require('./event/core');
const { validateOptions : validate } = require('./deprecated/api-utils');
const { Cc, Ci } = require('chrome');
const { merge } = require('./util/object');
const { readURISync } = require('./net/url');
const { windowIterator } = require('./deprecated/window-utils');
const { isBrowser, getFrames } = require('./window/utils');
const { getTabs, getTabContentWindow, getTabForContentWindow,
        getURI: getTabURI } = require('./tabs/utils');
const { ignoreWindow } = require('sdk/private-browsing/utils');
const { Style } = require("./stylesheet/style");
const { attach, detach } = require("./content/mod");
const { has, hasAny } = require("./util/array");
const { Rules } = require("./util/rules");

// Valid values for `attachTo` option
const VALID_ATTACHTO_OPTIONS = ['existing', 'top', 'frame'];

const mods = new WeakMap();

// contentStyle* / contentScript* are sharing the same validation constraints,
// so they can be mostly reused, except for the messages.
const validStyleOptions = {
  contentStyle: merge(Object.create(validationAttributes.contentScript), {
    msg: 'The `contentStyle` option must be a string or an array of strings.'
  }),
  contentStyleFile: merge(Object.create(validationAttributes.contentScriptFile), {
    msg: 'The `contentStyleFile` option must be a local URL or an array of URLs'
  })
};

/**
 * PageMod constructor (exported below).
 * @constructor
 */
const PageMod = Loader.compose(EventEmitter, {
  on: EventEmitter.required,
  _listeners: EventEmitter.required,
  attachTo: [],
  contentScript: Loader.required,
  contentScriptFile: Loader.required,
  contentScriptWhen: Loader.required,
  contentScriptOptions: Loader.required,
  include: null,
  constructor: function PageMod(options) {
    this._onContent = this._onContent.bind(this);
    options = options || {};

    let { contentStyle, contentStyleFile } = validate(options, validStyleOptions);

    if ('contentScript' in options)
      this.contentScript = options.contentScript;
    if ('contentScriptFile' in options)
      this.contentScriptFile = options.contentScriptFile;
    if ('contentScriptOptions' in options)
      this.contentScriptOptions = options.contentScriptOptions;
    if ('contentScriptWhen' in options)
      this.contentScriptWhen = options.contentScriptWhen;
    if ('onAttach' in options)
      this.on('attach', options.onAttach);
    if ('onError' in options)
      this.on('error', options.onError);
    if ('attachTo' in options) {
      if (typeof options.attachTo == 'string')
        this.attachTo = [options.attachTo];
      else if (Array.isArray(options.attachTo))
        this.attachTo = options.attachTo;
      else
        throw new Error('The `attachTo` option must be a string or an array ' +
                        'of strings.');

      let isValidAttachToItem = function isValidAttachToItem(item) {
        return typeof item === 'string' &&
               VALID_ATTACHTO_OPTIONS.indexOf(item) !== -1;
      }
      if (!this.attachTo.every(isValidAttachToItem))
        throw new Error('The `attachTo` option valid accept only following ' +
                        'values: '+ VALID_ATTACHTO_OPTIONS.join(', '));
      if (!hasAny(this.attachTo, ["top", "frame"]))
        throw new Error('The `attachTo` option must always contain at least' +
                        ' `top` or `frame` value');
    }
    else {
      this.attachTo = ["top", "frame"];
    }

    let include = options.include;
    let rules = this.include = Rules();

    if (!include)
      throw new Error('The `include` option must always contain atleast one rule');

    rules.add.apply(rules, [].concat(include));

    if (contentStyle || contentStyleFile) {
      this._style = Style({
        uri: contentStyleFile,
        source: contentStyle
      });
    }

    this.on('error', this._onUncaughtError = this._onUncaughtError.bind(this));
    pageModManager.add(this._public);
    mods.set(this._public, this);

    // `_applyOnExistingDocuments` has to be called after `pageModManager.add()`
    // otherwise its calls to `_onContent` method won't do anything.
    if ('attachTo' in options && has(options.attachTo, 'existing'))
      this._applyOnExistingDocuments();
  },

  destroy: function destroy() {
    if (this._style)
      detach(this._style);

    for (let i in this.include)
      this.include.remove(this.include[i]);

    mods.delete(this._public);
    pageModManager.remove(this._public);
  },

  _applyOnExistingDocuments: function _applyOnExistingDocuments() {
    let mod = this;
    let tabs = getAllTabs();

    tabs.forEach(function (tab) {
      // Fake a newly created document
      let window = getTabContentWindow(tab);
      if (has(mod.attachTo, "top") && mod.include.matchesAny(getTabURI(tab)))
        mod._onContent(window);
      if (has(mod.attachTo, "frame")) {
        getFrames(window).
            filter((iframe) => mod.include.matchesAny(iframe.location.href)).
            forEach(mod._onContent);
      }
    });
  },

  _onContent: function _onContent(window) {
    // not registered yet
    if (!pageModManager.has(this))
      return;

    let isTopDocument = window.top === window;
    // Is a top level document and `top` is not set, ignore
    if (isTopDocument && !has(this.attachTo, "top"))
      return;
    // Is a frame document and `frame` is not set, ignore
    if (!isTopDocument && !has(this.attachTo, "frame"))
      return;

    if (this._style)
      attach(this._style, window);

    // Immediatly evaluate content script if the document state is already
    // matching contentScriptWhen expectations
    let state = window.document.readyState;
    if ('start' === this.contentScriptWhen ||
        // Is `load` event already dispatched?
        'complete' === state ||
        // Is DOMContentLoaded already dispatched and waiting for it?
        ('ready' === this.contentScriptWhen && state === 'interactive') ) {
      this._createWorker(window);
      return;
    }

    let eventName = 'end' == this.contentScriptWhen ? 'load' : 'DOMContentLoaded';
    let self = this;
    window.addEventListener(eventName, function onReady(event) {
      if (event.target.defaultView != window)
        return;
      window.removeEventListener(eventName, onReady, true);

      self._createWorker(window);
    }, true);
  },
  _createWorker: function _createWorker(window) {
    let worker = Worker({
      window: window,
      contentScript: this.contentScript,
      contentScriptFile: this.contentScriptFile,
      contentScriptOptions: this.contentScriptOptions,
      onError: this._onUncaughtError
    });
    this._emit('attach', worker);
    let self = this;
    worker.once('detach', function detach() {
      worker.destroy();
    });
  },
  _onUncaughtError: function _onUncaughtError(e) {
    if (this._listeners('error').length == 1)
      console.exception(e);
  }
});
exports.PageMod = function(options) PageMod(options)
exports.PageMod.prototype = PageMod.prototype;

const PageModManager = Registry.resolve({
  constructor: '_init',
  _destructor: '_registryDestructor'
}).compose({
  constructor: function PageModRegistry(constructor) {
    this._init(PageMod);
    observers.on(
      'document-element-inserted',
      this._onContentWindow = this._onContentWindow.bind(this)
    );
  },
  _destructor: function _destructor() {
    observers.off('document-element-inserted', this._onContentWindow);
    this._removeAllListeners();

    // We need to do some cleaning er PageMods, like unregistering any
    // `contentStyle*`
    this._registry.forEach(function(pageMod) {
      pageMod.destroy();
    });

    this._registryDestructor();
  },
  _onContentWindow: function _onContentWindow({ subject: document }) {
    let window = document.defaultView;
    // XML documents don't have windows, and we don't yet support them.
    if (!window)
      return;
    // We apply only on documents in tabs of Firefox
    if (!getTabForContentWindow(window))
      return;

    // When the tab is private, only addons with 'private-browsing' flag in
    // their package.json can apply content script to private documents
    if (ignoreWindow(window)) {
      return;
    }

    this._registry.forEach(function(mod) {
      if (mod.include.matchesAny(document.URL))
        mods.get(mod)._onContent(window);
    });
  },
  off: function off(topic, listener) {
    this.removeListener(topic, listener);
  }
});
const pageModManager = PageModManager();

// Returns all tabs on all currently opened windows
function getAllTabs() {
  let tabs = [];
  // Iterate over all chrome windows
  for (let window in windowIterator()) {
    if (!isBrowser(window))
      continue;
    tabs = tabs.concat(getTabs(window));
  }
  return tabs;
}
