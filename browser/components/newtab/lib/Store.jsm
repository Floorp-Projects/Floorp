/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { ActivityStreamMessageChannel } = ChromeUtils.import(
  "resource://activity-stream/lib/ActivityStreamMessageChannel.jsm"
);
const { ActivityStreamStorage } = ChromeUtils.import(
  "resource://activity-stream/lib/ActivityStreamStorage.jsm"
);
const { Prefs } = ChromeUtils.importESModule(
  "resource://activity-stream/lib/ActivityStreamPrefs.sys.mjs"
);
const { reducers } = ChromeUtils.importESModule(
  "resource://activity-stream/common/Reducers.sys.mjs"
);
const { redux } = ChromeUtils.importESModule(
  "resource://activity-stream/vendor/Redux.sys.mjs"
);

/**
 * Store - This has a similar structure to a redux store, but includes some extra
 *         functionality to allow for routing of actions between the Main processes
 *         and child processes via a ActivityStreamMessageChannel.
 *         It also accepts an array of "Feeds" on inititalization, which
 *         can listen for any action that is dispatched through the store.
 */
class Store {
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
    this._messageChannel = new ActivityStreamMessageChannel({
      dispatch: this.dispatch,
    });
    this._store = redux.createStore(
      redux.combineReducers(reducers),
      redux.applyMiddleware(this._middleware, this._messageChannel.middleware)
    );
    this.storage = null;
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
   * @param {Action} initAction An optional action to initialize the feed
   */
  initFeed(feedName, initAction) {
    const feed = this._feedFactories.get(feedName)();
    feed.store = this;
    this.feeds.set(feedName, feed);
    if (initAction && feed.onAction) {
      feed.onAction(initAction);
    }
  }

  /**
   * uninitFeed - Removes a feed and calls its uninit function if defined
   *
   * @param  {string} feedName The name of a feed, as defined in the object
   *                           passed to Store.init
   * @param {Action} uninitAction An optional action to uninitialize the feed
   */
  uninitFeed(feedName, uninitAction) {
    const feed = this.feeds.get(feedName);
    if (!feed) {
      return;
    }
    if (uninitAction && feed.onAction) {
      feed.onAction(uninitAction);
    }
    this.feeds.delete(feedName);
  }

  /**
   * onPrefChanged - Listener for handling feed changes.
   */
  onPrefChanged(name, value) {
    if (this._feedFactories.has(name)) {
      if (value) {
        this.initFeed(name, this._initAction);
      } else {
        this.uninitFeed(name, this._uninitAction);
      }
    }
  }

  /**
   * init - Initializes the ActivityStreamMessageChannel channel, and adds feeds.
   *
   * Note that it intentionally initializes the TelemetryFeed first so that the
   * addon is able to report the init errors from other feeds.
   *
   * @param  {Map} feedFactories A Map of feeds with the name of the pref for
   *                                the feed as the key and a function that
   *                                constructs an instance of the feed.
   * @param {Action} initAction An optional action that will be dispatched
   *                            to feeds when they're created.
   * @param {Action} uninitAction An optional action for when feeds uninit.
   */
  async init(feedFactories, initAction, uninitAction) {
    this._feedFactories = feedFactories;
    this._initAction = initAction;
    this._uninitAction = uninitAction;

    const telemetryKey = "feeds.telemetry";
    if (feedFactories.has(telemetryKey) && this._prefs.get(telemetryKey)) {
      this.initFeed(telemetryKey);
    }

    await this._initIndexedDB(telemetryKey);

    for (const pref of feedFactories.keys()) {
      if (pref !== telemetryKey && this._prefs.get(pref)) {
        this.initFeed(pref);
      }
    }

    this._prefs.observeBranch(this);

    // Dispatch an initial action after all enabled feeds are ready
    if (initAction) {
      this.dispatch(initAction);
    }

    // Dispatch NEW_TAB_INIT/NEW_TAB_LOAD events after INIT event.
    this._messageChannel.simulateMessagesForExistingTabs();
  }

  async _initIndexedDB(telemetryKey) {
    // "snippets" is the name of one storage space, but these days it is used
    // not for snippet-related data (snippets were removed in bug 1715158),
    // but storage for impression or session data for all ASRouter messages.
    //
    // We keep the name "snippets" to avoid having to do an IndexedDB database
    // migration.
    this.dbStorage = new ActivityStreamStorage({
      storeNames: ["sectionPrefs", "snippets"],
    });
    // Accessing the db causes the object stores to be created / migrated.
    // This needs to happen before other instances try to access the db, which
    // would update only a subset of the stores to the latest version.
    try {
      await this.dbStorage.db; // eslint-disable-line no-unused-expressions
    } catch (e) {
      this.dbStorage.telemetry = null;
    }
  }

  /**
   * uninit -  Uninitalizes each feed, clears them, and destroys the message
   *           manager channel.
   *
   * @return {type}  description
   */
  uninit() {
    if (this._uninitAction) {
      this.dispatch(this._uninitAction);
    }
    this._prefs.ignoreBranch(this);
    this.feeds.clear();
    this._feedFactories = null;
  }

  /**
   * getMessageChannel - Used by the AboutNewTabParent actor to get the message channel.
   */
  getMessageChannel() {
    return this._messageChannel;
  }
}

const EXPORTED_SYMBOLS = ["Store"];
