/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {utils: Cu} = Components;

const {redux} = Cu.import("resource://activity-stream/vendor/Redux.jsm", {});
const {actionTypes: at} = Cu.import("resource://activity-stream/common/Actions.jsm", {});
const {reducers} = Cu.import("resource://activity-stream/common/Reducers.jsm", {});
const {ActivityStreamMessageChannel} = Cu.import("resource://activity-stream/lib/ActivityStreamMessageChannel.jsm", {});

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
    this.feeds = new Set();
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
   * init - Initializes the ActivityStreamMessageChannel channel, and adds feeds.
   *        After initialization has finished, an INIT action is dispatched.
   *
   * @param  {array} feeds An array of objects with an optional .onAction method
   */
  init(feeds) {
    if (feeds) {
      feeds.forEach(subscriber => {
        subscriber.store = this;
        this.feeds.add(subscriber);
      });
    }
    this._messageChannel.createChannel();
    this.dispatch({type: at.INIT});
  }

  /**
   * uninit - Clears all feeds, dispatches an UNINIT action, and
   *          destroys the message manager channel.
   *
   * @return {type}  description
   */
  uninit() {
    this.feeds.clear();
    this.dispatch({type: at.UNINIT});
    this._messageChannel.destroyChannel();
  }
};

this.EXPORTED_SYMBOLS = ["Store"];
