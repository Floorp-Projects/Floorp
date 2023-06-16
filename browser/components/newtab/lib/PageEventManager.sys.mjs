/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Methods for setting up and tearing down page event listeners. These are used
 * to dismiss Feature Callouts when the callout's anchor element is clicked.
 */
export class PageEventManager {
  /**
   * A set of parameters defining a page event listener.
   * @typedef {Object} PageEventListenerParams
   * @property {String} type Event type string e.g. `click`
   * @property {String} selectors Target selector, e.g. `tag.class, #id[attr]`
   * @property {PageEventListenerOptions} [options] addEventListener options
   *
   * @typedef {Object} PageEventListenerOptions
   * @property {Boolean} [capture] Use event capturing phase?
   * @property {Boolean} [once] Remove listener after first event?
   * @property {Boolean} [preventDefault] Inverted value for `passive` option
   */

  /**
   * Maps event listener params to their abort controllers.
   * @type {Map<PageEventListenerParams, AbortController>}
   */
  _listeners = new Map();

  /**
   * @param {Document} doc The document to look for event targets in
   */
  constructor(doc) {
    this.doc = doc;
  }

  /**
   * Adds a page event listener.
   * @param {PageEventListenerParams} params
   * @param {Function} callback Function to call when event is triggered
   */
  on(params, callback) {
    if (this._listeners.has(params)) {
      return;
    }
    const { type, selectors, options = {} } = params;
    const controller = new AbortController();
    const opt = {
      capture: !!options.capture,
      passive: !options.preventDefault,
      signal: controller.signal,
    };
    const targets = this.doc.querySelectorAll(selectors);
    for (const target of targets) {
      target.addEventListener(type, callback, opt);
    }
    this._listeners.set(params, controller);
  }

  /**
   * Removes a page event listener.
   * @param {PageEventListenerParams} params
   */
  off(params) {
    const controller = this._listeners.get(params);
    if (!controller) {
      return;
    }
    controller.abort();
    this._listeners.delete(params);
  }

  /**
   * Adds a page event listener that is removed after the first event.
   * @param {PageEventListenerParams} params
   * @param {Function} callback Function to call when event is triggered
   */
  once(params, callback) {
    const wrappedCallback = (...args) => {
      this.off(params);
      callback(...args);
    };
    this.on(params, wrappedCallback);
  }

  /**
   * Removes all page event listeners.
   */
  clear() {
    for (const controller of this._listeners.values()) {
      controller.abort();
    }
    this._listeners.clear();
  }
}
