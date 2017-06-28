/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;

const {ActivityStreamMessageChannel} = Cu.import("resource://activity-stream/lib/ActivityStreamMessageChannel.jsm", {});
const {Prefs} = Cu.import("resource://activity-stream/lib/ActivityStreamPrefs.jsm", {});
const {reducers} = Cu.import("resource://activity-stream/common/Reducers.jsm", {});
const {redux} = Cu.import("resource://activity-stream/vendor/Redux.jsm", {});

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
    for (const method of ["dispatch", "getState", "subscribe"]) {
      this[method] = (...args) => this._store[method](...args);
    }
    this.feeds = new Map();
    this._prefs = new Prefs();
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
  _middleware() {
    return next => action => {
      next(action);
      for (const store of this.feeds.values()) {
        if (store.onAction) {
          store.onAction(action);
        }
      }
    };
  }

  /**
   * initFeed - Initializes a feed by calling its constructor function
   *
   * @param  {string} feedName The name of a feed, as defined in the object
   *                           passed to Store.init
   */
  initFeed(feedName) {
    const feed = this._feedFactories.get(feedName)();
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
   * onPrefChanged - Listener for handling feed changes.
   */
  onPrefChanged(name, value) {
    if (this._feedFactories.has(name)) {
      if (value) {
        this.initFeed(name);
      } else {
        this.uninitFeed(name);
      }
    }
  }

  /**
   * init - Initializes the ActivityStreamMessageChannel channel, and adds feeds.
   *
   * @param  {Map} feedFactories A Map of feeds with the name of the pref for
   *                                the feed as the key and a function that
   *                                constructs an instance of the feed.
   */
  init(feedFactories) {
    this._feedFactories = feedFactories;
    for (const pref of feedFactories.keys()) {
      if (this._prefs.get(pref)) {
        this.initFeed(pref);
      }
    }

    this._prefs.observeBranch(this);
    this._messageChannel.createChannel();
  }

  /**
   * uninit -  Uninitalizes each feed, clears them, and destroys the message
   *           manager channel.
   *
   * @return {type}  description
   */
  uninit() {
    this._prefs.ignoreBranch(this);
    this.feeds.forEach(feed => this.uninitFeed(feed));
    this.feeds.clear();
    this._feedFactories = null;
    this._messageChannel.destroyChannel();
  }
};

this.EXPORTED_SYMBOLS = ["Store"];
