/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci } = require('chrome'),
      { setTimeout } = require('../timers'),
      { Trait } = require('../deprecated/traits'),
      { openDialog } = require('../window/utils'),

      ON_LOAD = 'load',
      ON_UNLOAD = 'unload',
      STATE_LOADED = 'complete';

/**
 * Trait provides private `_window` property and requires `_onLoad` property
 * that will be called when `_window` is loaded. If `_window` property value
 * is changed with already loaded window `_onLoad` still will be called.
 */
const WindowLoader = Trait.compose({
  /**
   * Internal listener that is called when window is loaded.
   * Please keep in mind that this trait will not handle exceptions that may
   * be thrown by this method so method itself should take care of
   * handling them.
   * @param {nsIWindow} window
   */
  _onLoad: Trait.required,
  _tabOptions: Trait.required,
  /**
   * Internal listener that is called when `_window`'s DOM 'unload' event
   * is dispatched. Please note that this trait will not handle exceptions that
   * may be thrown by this method so method itself should take care of
   * handling them.
   */
  _onUnload: Trait.required,
  _load: function _load() {
    if (this.__window)
      return;

    this._window = openDialog({
      private: this._isPrivate,
      args: this._tabOptions.map(function(options) options.url).join("|")
    });
  },
  /**
   * Private window who's load event is being tracked. Once window is loaded
   * `_onLoad` is called.
   * @type {nsIWindow}
   */
  get _window() this.__window,
  set _window(window) {
    let _window = this.__window;
    if (!window) window = null;

    if (window !== _window) {
      if (_window) {
        _window.removeEventListener(ON_UNLOAD, this.__unloadListener, false);
        _window.removeEventListener(ON_LOAD, this.__loadListener, false);
      }

      if (window) {
        window.addEventListener(
          ON_UNLOAD,
          this.__unloadListener ||
            (this.__unloadListener = this._unloadListener.bind(this))
          ,
          false
        );

        this.__window = window;

        // If window is not loaded yet setting up a listener.
        if (STATE_LOADED != window.document.readyState) {
          window.addEventListener(
            ON_LOAD,
            this.__loadListener ||
              (this.__loadListener = this._loadListener.bind(this))
            ,
            false
          );
        }
        else { // If window is loaded calling listener next turn of event loop.
          this._onLoad(window)
        }
      }
      else {
        this.__window = null;
      }
    }
  },
  __window: null,
  /**
   * Internal method used for listening 'load' event on the `_window`.
   * Method takes care of removing itself from 'load' event listeners once
   * event is being handled.
   */
  _loadListener: function _loadListener(event) {
    let window = this._window;
    if (!event.target || event.target.defaultView != window) return;
    window.removeEventListener(ON_LOAD, this.__loadListener, false);
    this._onLoad(window);
  },
  __loadListener: null,
  /**
   * Internal method used for listening 'unload' event on the `_window`.
   * Method takes care of removing itself from 'unload' event listeners once
   * event is being handled.
   */
  _unloadListener: function _unloadListener(event) {
    let window = this._window;
    if (!event.target
      || event.target.defaultView != window
      || STATE_LOADED != window.document.readyState
    ) return;
    window.removeEventListener(ON_UNLOAD, this.__unloadListener, false);
    this._onUnload(window);
  },
  __unloadListener: null
});
exports.WindowLoader = WindowLoader;

