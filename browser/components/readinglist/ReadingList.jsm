/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ReadingList"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;


Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Log.jsm");


(function() {
  let parentLog = Log.repository.getLogger("readinglist");
  parentLog.level = Preferences.get("browser.readinglist.logLevel", Log.Level.Warn);
  Preferences.observe("browser.readinglist.logLevel", value => {
    parentLog.level = value;
  });
  let formatter = new Log.BasicFormatter();
  parentLog.addAppender(new Log.ConsoleAppender(formatter));
  parentLog.addAppender(new Log.DumpAppender(formatter));
})();

let log = Log.repository.getLogger("readinglist.api");

/**
 * Represents an item in the Reading List.
 * @constructor
 * @see https://github.com/mozilla-services/readinglist/wiki/API-Design-proposal#data-model
 */
function Item(data) {
  this._data = data;
}

Item.prototype = {
  /**
   * UUID
   * @type {string}
   */
  get id() {
    return this._data.id;
  },

  /**
   * Server timestamp
   * @type {string}
   */
  get lastModified() {
    return this._data.last_modified;
  },

  /**
   * @type {nsIURL}
   */
  get originalUrl() {
    return Services.io.newURI(this._data.url, null, null);
  },

  /**
   * @type {string}
   */
  get originalTitle() {
    return this._data.title || "";
  },

  /**
   * @type {nsIURL}
   */
  get resolvedUrl() {
    return Services.io.newURI(this._data.resolved_url || this._data.url, null, null);
  },

  /**
   * @type {string}
   */
  get resolvedTitle() {
    return this._data.resolved_title || this.originalTitle;
  },

  /**
   * @type {string}
   */
  get excerpt() {
    return this._data.excerpt || "";
  },

  /**
   * @type {ItemStates}
   */
  get state() {
    return ReadingList.ItemStates[this._data.state] || ReadingList.ItemStates.OK;
  },

  /**
   * @type {boolean}
   */
  get isFavorite() {
    return !!this._data.favorite;
  },

  /**
   * @type {boolean}
   */
  get isArticle() {
    return !!this._data.is_article;
  },

  /**
   * @type {number}
   */
  get wordCount() {
    return this._data.word_count || 0;
  },

  /**
   * @type {boolean}
   */
  get isUnread() {
    return !!this._data.unread;
  },

  /**
   * Device name
   * @type {string}
   */
  get addedBy() {
    return this._data.added_by;
  },

  /**
   * @type {Date}
   */
  get addedOn() {
    return new Date(this._data.added_on);
  },

  /**
   * @type {Date}
   */
  get storedOn() {
    return new Date(this._data.stored_on);
  },

  /**
   * Device name
   * @type {string}
   */
  get markedReadBy() {
    return this._data.marked_read_by;
  },

  /**
   * @type {Date}
   */
  get markedReadOn() {
    return new date(this._data.marked_read_on);
  },

  /**
   * @type {number}
   */
  get readPosition() {
    return this._data.read_position;
  },

  // Data not specified by the current server API

  /**
   * Array of scraped or captured summary images for this page.
   * TODO: Implement this.
   * @type {[nsIURL]}
   */
  get images() {
    return [];
  },

  /**
   * Favicon for this site.
   * @type {nsIURL}
   * TODO: Generate moz-anno: URI for favicon.
   */
  get favicon() {
    return null;
  },

  // Helpers

  /**
   * Alias for resolvedUrl.
   * TODO: This url/resolvedUrl alias makes it feel like the server API hasn't got this right.
   */
  get url() {
    return this.resolvedUrl;
  },
  /**
   * Alias for resolvedTitle
   */
  get title() {
    return this.resolvedTitle;
  },

  /**
   * Domain portion of the URL, with prefixes stripped. For display purposes.
   * @type {string}
   */
  get domain() {
    let host = this.resolvedUrl.host;
    if (host.startsWith("www.")) {
      host = host.slice(4);
    }
    return host;
  },

  /**
   * Convert this Item to a string representation.
   */
  toString() {
    return `[Item url=${this.url.spec}]`;
  },

  /**
   * Get the value that should be used for a JSON representation of this Item.
   */
  toJSON() {
    return this._data;
  },
};


let ItemStates = {
  OK: Symbol("ok"),
  ARCHIVED: Symbol("archived"),
  DELETED: Symbol("deleted"),
};


this.ReadingList = {
  Item: Item,
  ItemStates: ItemStates,

  _listeners: new Set(),
  _items: [],

  /**
   * Initialize the ReadingList component.
   */
  _init() {
    log.debug("Init");

    // Initialize mock data
    let mockData = JSON.parse(Preferences.get("browser.readinglist.mockData", "[]"));
    for (let itemData of mockData) {
      this._items.push(new Item(itemData));
    }
  },

  /**
   * Add an event listener.
   * @param {object} listener - Listener object to start notifying.
   */
  addListener(listener) {
    this._listeners.add(listener);
  },

  /**
   * Remove a specified event listener.
   * @param {object} listener - Listener object to stop notifying.
   */
  removeListener(listener) {
    this._listeners.delete(listener);
  },

  /**
   * Notify all registered event listeners of an event.
   * @param {string} eventName - Event name, which will be used as a method name
   *                             on listeners to call.
   */
  _notifyListeners(eventName, ...args) {
    for (let listener of this._listeners) {
      if (typeof listener[eventName] != "function") {
        continue;
      }

      try {
        listener[eventName](...args);
      } catch (e) {
        log.error(`Error calling listener.${eventName}`, e);
      }
    }
  },

  /**
   * Fetch the number of items that match a set of given conditions.
   * TODO: Implement filtering, sorting, etc. Needs backend storage work.
   *
   * @param {Object} conditions Object specifying a set of conditions for
   *                            filtering items.
   * @return {Promise}
   * @resolves {number}
   */
  getNumItems(conditions = {unread: false}) {
    return new Promise((resolve, reject) => {
      resolve(this._items.length);
    });
  },

  /**
   * Fetch items matching a set of conditions, in a sorted list.
   * TODO: Implement filtering, sorting, etc. Needs backend storage work.
   *
   * @return {Promise}
   * @resolves {[Item]}
   */
  getItems(options = {sort: "addedOn", conditions: {unread: false}}) {
    return new Promise((resolve, reject) => {
      resolve([...this._items]);
    });
  },

  /**
   * Find an item based on its ID.
   * TODO: Implement. Needs backend storage work.
   *
   * @return {Promise}
   * @resolves {Item}
   */
  getItemByID(url) {
    return new Promise((resolve, reject) => {
      resolve(null);
    });
  },

  /**
   * Find an item based on its URL.
   *
   * TODO: Implement. Needs backend storage work.
   * TODO: Does this match original or resolved URL, or both?
   * TODO: Should this just be a generic findItem API?
   *
   * @return {Promise}
   * @resolves {Item}
   */
  getItemByURL(url) {
    return new Promise((resolve, reject) => {
      resolve(null);
    });
  },
};


ReadingList._init();
