/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cc, Ci, Cr } = require('chrome'),
      { Trait } = require('../deprecated/traits'),
      { List } = require('../deprecated/list'),
      { EventEmitter } = require('../deprecated/events'),
      { WindowTabs, WindowTabTracker } = require('./tabs-firefox'),
      { WindowDom } = require('./dom'),
      { WindowLoader } = require('./loader'),
      { isBrowser, getWindowDocShell, windows: windowIterator } = require('../window/utils'),
      { Options } = require('../tabs/common'),
      apiUtils = require('../deprecated/api-utils'),
      unload = require('../system/unload'),
      windowUtils = require('../deprecated/window-utils'),
      { WindowTrackerTrait } = windowUtils,
      { ns } = require('../core/namespace'),
      { observer: windowObserver } = require('./observer'),
      { getOwnerWindow } = require('../private-browsing/window/utils');
const { windowNS } = require('../window/namespace');
const { isPrivateBrowsingSupported } = require('../self');
const { ignoreWindow } = require('sdk/private-browsing/utils');
const { viewFor } = require('../view/core');

/**
 * Window trait composes safe wrappers for browser window that are E10S
 * compatible.
 */
const BrowserWindowTrait = Trait.compose(
  EventEmitter,
  WindowDom.resolve({ close: '_close' }),
  WindowTabs,
  WindowTabTracker,
  WindowLoader,
  /* WindowSidebars, */
  Trait.compose({
    _emit: Trait.required,
    _close: Trait.required,
    _load: Trait.required,
    /**
     * Constructor returns wrapper of the specified chrome window.
     * @param {nsIWindow} window
     */
    constructor: function BrowserWindow(options) {
      // Register this window ASAP, in order to avoid loop that would try
      // to create this window instance over and over (see bug 648244)
      windows.push(this);

      // make sure we don't have unhandled errors
      this.on('error', console.exception.bind(console));

      if ('onOpen' in options)
        this.on('open', options.onOpen);
      if ('onClose' in options)
        this.on('close', options.onClose);
      if ('onActivate' in options)
        this.on('activate', options.onActivate);
      if ('onDeactivate' in options)
        this.on('deactivate', options.onDeactivate);
      if ('window' in options)
        this._window = options.window;

      if ('tabs' in options) {
        this._tabOptions = Array.isArray(options.tabs) ?
                           options.tabs.map(Options) :
                           [ Options(options.tabs) ];
      }
      else if ('url' in options) {
        this._tabOptions = [ Options(options.url) ];
      }

      this._isPrivate = isPrivateBrowsingSupported && !!options.isPrivate;

      this._load();

      windowNS(this._public).window = this._window;
      getOwnerWindow.implement(this._public, getChromeWindow);
      viewFor.implement(this._public, getChromeWindow);

      return this;
    },
    destroy: function () this._onUnload(),
    _tabOptions: [],
    _onLoad: function() {
      try {
        this._initWindowTabTracker();
        this._loaded = true;
      }
      catch(e) {
        this._emit('error', e);
      }

      this._emitOnObject(browserWindows, 'open', this._public);
    },
    _onUnload: function() {
      if (!this._window)
        return;
      if (this._loaded)
        this._destroyWindowTabTracker();

      this._emitOnObject(browserWindows, 'close', this._public);
      this._window = null;
      windowNS(this._public).window = null;
      // Removing reference from the windows array.
      windows.splice(windows.indexOf(this), 1);
      this._removeAllListeners();
    },
    close: function close(callback) {
      // maybe we should deprecate this with message ?
      if (callback) this.on('close', callback);
      return this._close();
    }
  })
);

/**
 * Gets a `BrowserWindowTrait` for the given `chromeWindow` if previously
 * registered, `null` otherwise.
 */
function getRegisteredWindow(chromeWindow) {
  for each (let window in windows) {
    if (chromeWindow === window._window)
      return window;
  }

  return null;
}

/**
 * Wrapper for `BrowserWindowTrait`. Creates new instance if wrapper for
 * window doesn't exists yet. If wrapper already exists then returns it
 * instead.
 * @params {Object} options
 *    Options that are passed to the the `BrowserWindowTrait`
 * @returns {BrowserWindow}
 * @see BrowserWindowTrait
 */
function BrowserWindow(options) {
  let window = null;

  if ("window" in options)
    window = getRegisteredWindow(options.window);

  return (window || BrowserWindowTrait(options))._public;
}
// to have proper `instanceof` behavior will go away when #596248 is fixed.
BrowserWindow.prototype = BrowserWindowTrait.prototype;
exports.BrowserWindow = BrowserWindow;

const windows = [];

const browser = ns();

function onWindowActivation (chromeWindow, event) {
  if (!isBrowser(chromeWindow)) return; // Ignore if it's not a browser window.

  let window = getRegisteredWindow(chromeWindow);

  if (window)
    window._emit(event.type, window._public);
  else
    window = BrowserWindowTrait({ window: chromeWindow });

  browser(browserWindows).internals._emit(event.type, window._public);
}

windowObserver.on("activate", onWindowActivation);
windowObserver.on("deactivate", onWindowActivation);

/**
 * `BrowserWindows` trait is composed out of `List` trait and it represents
 * "live" list of currently open browser windows. Instance mutates itself
 * whenever new browser window gets opened / closed.
 */
// Very stupid to resolve all `toStrings` but this will be fixed by #596248
const browserWindows = Trait.resolve({ toString: null }).compose(
  List.resolve({ constructor: '_initList' }),
  EventEmitter.resolve({ toString: null }),
  WindowTrackerTrait.resolve({ constructor: '_initTracker', toString: null }),
  Trait.compose({
    _emit: Trait.required,
    _add: Trait.required,
    _remove: Trait.required,

    // public API

    /**
     * Constructor creates instance of `Windows` that represents live list of open
     * windows.
     */
    constructor: function BrowserWindows() {
      browser(this._public).internals = this;

      this._trackedWindows = [];
      this._initList();
      this._initTracker();
      unload.ensure(this, "_destructor");
    },
    _destructor: function _destructor() {
      this._removeAllListeners('open');
      this._removeAllListeners('close');
      this._removeAllListeners('activate');
      this._removeAllListeners('deactivate');
      this._clear();

      delete browser(this._public).internals;
    },
    /**
     * This property represents currently active window.
     * Property is non-enumerable, in order to preserve array like enumeration.
     * @type {Window|null}
     */
    get activeWindow() {
      let window = windowUtils.activeBrowserWindow;
      // Bug 834961: ignore private windows when they are not supported
      if (ignoreWindow(window))
        window = windowIterator()[0];
      return window ? BrowserWindow({window: window}) : null;
    },
    open: function open(options) {
      if (typeof options === "string") {
        // `tabs` option is under review and may be removed.
        options = {
          tabs: [Options(options)],
          isPrivate: isPrivateBrowsingSupported && options.isPrivate
        };
      }
      return BrowserWindow(options);
    },

     /**
      * Internal listener which is called whenever new window gets open.
      * Creates wrapper and adds to this list.
      * @param {nsIWindow} chromeWindow
      */
    _onTrack: function _onTrack(chromeWindow) {
      if (!isBrowser(chromeWindow)) return;
      let window = BrowserWindow({ window: chromeWindow });
      this._add(window);
      this._emit('open', window);
    },

    /**
     * Internal listener which is called whenever window gets closed.
     * Cleans up references and removes wrapper from this list.
     * @param {nsIWindow} window
     */
    _onUntrack: function _onUntrack(chromeWindow) {
      if (!isBrowser(chromeWindow)) return;
      let window = BrowserWindow({ window: chromeWindow });
      this._remove(window);
      this._emit('close', window);

      // Bug 724404: do not leak this module and linked windows:
      // We have to do it on untrack and not only when `_onUnload` is called
      // when windows are closed, otherwise, we will leak on addon disabling.
      window.destroy();
    }
  }).resolve({ toString: null })
)();

function getChromeWindow(window) {
  return windowNS(window).window;
}

exports.browserWindows = browserWindows;
