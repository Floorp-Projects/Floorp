/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* global Preferences */
"use strict";

const {utils: Cu} = Components;

const {redux} = Cu.import("resource://activity-stream/vendor/Redux.jsm", {});
const {reducers} = Cu.import("resource://activity-stream/common/Reducers.jsm", {});
const {ActivityStreamMessageChannel} = Cu.import("resource://activity-stream/lib/ActivityStreamMessageChannel.jsm", {});

const PREF_PREFIX = "browser.newtabpage.activity-stream.";
Cu.import("resource://gre/modules/Preferences.jsm");

/**
 * Store - This has a similar structure to a redux store, but includes some extra
 *         functionality to allow for routing of actions between the Main processes
 *         and child processes via a ActivityStreamMessageChannel.
 *         It also accepts an array of "Feeds" on inititalization, which
 *         can listen for any action that is dispatched through the store.
 */
this.Store = class Store {

  /**
   * constructor - The redux store and message manager are created here,
   *               but no listeners are added until "init" is called.
   */
  constructor() {
    this._middleware = this._middleware.bind(this);
    // Bind each redux method so we can call it directly from the Store. E.g.,
    // store.dispatch() will call store._store.dispatch();
    ["dispatch", "getState", "subscribe"].forEach(method => {
      this[method] = function(...args) {
        return this._store[method](...args);
      }.bind(this);
    });
    this.feeds = new Map();
    this._feedFactories = null;
    this._prefHandlers = new Map();
    this._messageChannel = new ActivityStreamMessageChannel({dispatch: this.dispatch});
    this._store = redux.createStore(
      redux.combineReducers(reducers),
      redux.applyMiddleware(this._middleware, this._messageChannel.middleware)
    );
  }

  /**
   * _middleware - This is redux middleware consumed by redux.createStore.
   *               it calls each feed's .onAction method, if one
   *               is defined.
   */
  _middleware(store) {
    return next => action => {
      next(action);
      this.feeds.forEach(s => s.onAction && s.onAction(action));
    };
  }

  /**
   * initFeed - Initializes a feed by calling its constructor function
   *
   * @param  {string} feedName The name of a feed, as defined in the object
   *                           passed to Store.init
   */
  initFeed(feedName) {
    const feed = this._feedFactories[feedName]();
    feed.store = this;
    this.feeds.set(feedName, feed);
  }

  /**
   * uninitFeed - Removes a feed and calls its uninit function if defined
   *
   * @param  {string} feedName The name of a feed, as defined in the object
   *                           passed to Store.init
   */
  uninitFeed(feedName) {
    const feed = this.feeds.get(feedName);
    if (!feed) {
      return;
    }
    if (feed.uninit) {
      feed.uninit();
    }
    this.feeds.delete(feedName);
  }

  /**
   * maybeStartFeedAndListenForPrefChanges - Listen for pref changes that turn a
   *     feed off/on, and as long as that pref was not explicitly set to
   *     false, initialize the feed immediately.
   *
   * @param  {string} name The name of a feed, as defined in the object passed
   *                       to Store.init
   */
  maybeStartFeedAndListenForPrefChanges(name) {
    const prefName = PREF_PREFIX + name;

    // If the pref was never set, set it to true by default.
    if (!Preferences.has(prefName)) {
      Preferences.set(prefName, true);
    }

    // Create a listener that turns the feed off/on based on changes
    // to the pref, and cache it so we can unlisten on shut-down.
    const onPrefChanged = isEnabled => (isEnabled ? this.initFeed(name) : this.uninitFeed(name));
    this._prefHandlers.set(prefName, onPrefChanged);
    Preferences.observe(prefName, onPrefChanged);

    // TODO: This should propbably be done in a generic pref manager for Activity Stream.
    // If the pref is true, start the feed immediately.
    if (Preferences.get(prefName)) {
      this.initFeed(name);
    }
  }

  /**
   * init - Initializes the ActivityStreamMessageChannel channel, and adds feeds.
   *
   * @param  {array} feeds An array of objects with an optional .onAction method
   */
  init(feedConstructors) {
    if (feedConstructors) {
      this._feedFactories = feedConstructors;
      for (const name of Object.keys(feedConstructors)) {
        this.maybeStartFeedAndListenForPrefChanges(name);
      }
    }
    this._messageChannel.createChannel();
  }

  /**
   * uninit -  Uninitalizes each feed, clears them, and destroys the message
   *           manager channel.
   *
   * @return {type}  description
   */
  uninit() {
    this.feeds.forEach(feed => this.uninitFeed(feed));
    this._prefHandlers.forEach((handler, pref) => Preferences.ignore(pref, handler));
    this._prefHandlers.clear();
    this._feedFactories = null;
    this.feeds.clear();
    this._messageChannel.destroyChannel();
  }
};

this.PREF_PREFIX = PREF_PREFIX;
this.EXPORTED_SYMBOLS = ["Store", "PREF_PREFIX"];
