/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "stable"
};

const observers = require('./deprecated/observer-service');
const { Loader, validationAttributes } = require('./content/loader');
const { Worker } = require('./content/worker');
const { EventEmitter } = require('./deprecated/events');
const { List } = require('./deprecated/list');
const { Registry } = require('./util/registry');
const { MatchPattern } = require('./page-mod/match-pattern');
const { validateOptions : validate } = require('./deprecated/api-utils');
const { Cc, Ci } = require('chrome');
const { merge } = require('./util/object');
const { readURISync } = require('./net/url');
const { windowIterator } = require('./deprecated/window-utils');
const { isBrowser, getFrames } = require('./window/utils');
const { getTabs, getTabContentWindow, getTabForContentWindow,
        getURI: getTabURI } = require('./tabs/utils');
const { has, hasAny } = require('./util/array');
const { ignoreWindow } = require('sdk/private-browsing/utils');
const { Style } = require("./stylesheet/style");
const { attach, detach } = require("./content/mod");

// Valid values for `attachTo` option
const VALID_ATTACHTO_OPTIONS = ['existing', 'top', 'frame'];

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

// rules registry
const RULES = {};

const Rules = EventEmitter.resolve({ toString: null }).compose(List, {
  add: function() Array.slice(arguments).forEach(function onAdd(rule) {
    if (this._has(rule))
      return;
    // registering rule to the rules registry
    if (!(rule in RULES))
      RULES[rule] = new MatchPattern(rule);
    this._add(rule);
    this._emit('add', rule);
  }.bind(this)),
  remove: function() Array.slice(arguments).forEach(function onRemove(rule) {
    if (!this._has(rule))
      return;
    this._remove(rule);
    this._emit('remove', rule);
  }.bind(this)),
});

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
    rules.on('add', this._onRuleAdd = this._onRuleAdd.bind(this));
    rules.on('remove', this._onRuleRemove = this._onRuleRemove.bind(this));

    if (Array.isArray(include))
      rules.add.apply(null, include);
    else
      rules.add(include);

    if (contentStyle || contentStyleFile) {
      this._style = Style({
        uri: contentStyleFile,
        source: contentStyle
      });
    }

    this.on('error', this._onUncaughtError = this._onUncaughtError.bind(this));
    pageModManager.add(this._public);

    // `_applyOnExistingDocuments` has to be called after `pageModManager.add()`
    // otherwise its calls to `_onContent` method won't do anything.
    if ('attachTo' in options && has(options.attachTo, 'existing'))
      this._applyOnExistingDocuments();
  },

  destroy: function destroy() {

    if (this._style)
      detach(this._style);

    for each (let rule in this.include)
      this.include.remove(rule);
    pageModManager.remove(this._public);
  },

  _applyOnExistingDocuments: function _applyOnExistingDocuments() {
    let mod = this;
    // Returns true if the tab match one rule
    function isMatchingURI(uri) {
      // Use Array.some as `include` isn't a native array
      return Array.some(mod.include, function (rule) {
        return RULES[rule].test(uri);
      });
    }
    let tabs = getAllTabs().filter(function (tab) {
      return isMatchingURI(getTabURI(tab));
    });

    tabs.forEach(function (tab) {
      // Fake a newly created document
      let window = getTabContentWindow(tab);
      if (has(mod.attachTo, "top"))
        mod._onContent(window);
      if (has(mod.attachTo, "frame"))
        getFrames(window).forEach(mod._onContent);
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
  _onRuleAdd: function _onRuleAdd(url) {
    pageModManager.on(url, this._onContent);
  },
  _onRuleRemove: function _onRuleRemove(url) {
    pageModManager.off(url, this._onContent);
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
    observers.add(
      'document-element-inserted',
      this._onContentWindow = this._onContentWindow.bind(this)
    );
  },
  _destructor: function _destructor() {
    observers.remove('document-element-inserted', this._onContentWindow);
    this._removeAllListeners();
    for (let rule in RULES) {
      delete RULES[rule];
    }

    // We need to do some cleaning er PageMods, like unregistering any
    // `contentStyle*`
    this._registry.forEach(function(pageMod) {
      pageMod.destroy();
    });

    this._registryDestructor();
  },
  _onContentWindow: function _onContentWindow(document) {
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

    for (let rule in RULES)
      if (RULES[rule].test(document.URL))
        this._emit(rule, window);
  },
  off: function off(topic, listener) {
    this.removeListener(topic, listener);
    if (!this._listeners(topic).length)
      delete RULES[topic];
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
