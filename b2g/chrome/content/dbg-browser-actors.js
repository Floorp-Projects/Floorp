/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';
/**
 * B2G-specific actors that extend BrowserRootActor and BrowserTabActor,
 * overriding some of their methods.
 */

/**
 * The function that creates the root actor. DebuggerServer expects to find this
 * function in the loaded actors in order to initialize properly.
 */
function createRootActor(connection) {
  return new DeviceRootActor(connection);
}

/**
 * Creates the root actor that client-server communications always start with.
 * The root actor is responsible for the initial 'hello' packet and for
 * responding to a 'listTabs' request that produces the list of currently open
 * tabs.
 *
 * @param connection DebuggerServerConnection
 *        The conection to the client.
 */
function DeviceRootActor(connection) {
  BrowserRootActor.call(this, connection);
  this.browser = Services.wm.getMostRecentWindow('navigator:browser');
}

DeviceRootActor.prototype = new BrowserRootActor();

/**
 * Disconnects the actor from the browser window.
 */
DeviceRootActor.prototype.disconnect = function DRA_disconnect() {
  let actor = this._tabActors.get(this.browser);
  if (actor) {
    actor.exit();
  }
};

/**
 * Handles the listTabs request.  Builds a list of actors for the single
 * tab (window) running in the process. The actors will survive
 * until at least the next listTabs request.
 */
DeviceRootActor.prototype.onListTabs = function DRA_onListTabs() {
  let actor = this._tabActors.get(this.browser);
  if (!actor) {
    actor = new DeviceTabActor(this.conn, this.browser);
    // this.actorID is set by ActorPool when an actor is put into one.
    actor.parentID = this.actorID;
    this._tabActors.set(this.browser, actor);
  }

  let actorPool = new ActorPool(this.conn);
  actorPool.addActor(actor);

  // Now drop the old actorID -> actor map. Actors that still mattered were
  // added to the new map, others will go away.
  if (this._tabActorPool) {
    this.conn.removeActorPool(this._tabActorPool);
  }
  this._tabActorPool = actorPool;
  this.conn.addActorPool(this._tabActorPool);

  return {
    'from': 'root',
    'selected': 0,
    'tabs': [actor.grip()]
  };
};

/**
 * The request types this actor can handle.
 */
DeviceRootActor.prototype.requestTypes = {
  'listTabs': DeviceRootActor.prototype.onListTabs
};

/**
 * Creates a tab actor for handling requests to the single tab, like attaching
 * and detaching.
 *
 * @param connection DebuggerServerConnection
 *        The connection to the client.
 * @param browser browser
 *        The browser instance that contains this tab.
 */
function DeviceTabActor(connection, browser) {
  BrowserTabActor.call(this, connection, browser);
}

DeviceTabActor.prototype = new BrowserTabActor();

DeviceTabActor.prototype.grip = function DTA_grip() {
  dbg_assert(!this.exited,
             'grip() should not be called on exited browser actor.');
  dbg_assert(this.actorID,
             'tab should have an actorID.');
  return {
    'actor': this.actorID,
    'title': this.browser.title,
    'url': this.browser.document.documentURI
  }
};

/**
 * Creates a thread actor and a pool for context-lifetime actors. It then sets
 * up the content window for debugging.
 */
DeviceTabActor.prototype._pushContext = function DTA_pushContext() {
  dbg_assert(!this._contextPool, "Can't push multiple contexts");

  this._contextPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._contextPool);

  this.threadActor = new ThreadActor(this);
  this._addDebuggees(this.browser.wrappedJSObject);
  this._contextPool.addActor(this.threadActor);
};

// Protocol Request Handlers

/**
 * Prepare to enter a nested event loop by disabling debuggee events.
 */
DeviceTabActor.prototype.preNest = function DTA_preNest() {
  let windowUtils = this.browser
                        .QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils);
  windowUtils.suppressEventHandling(true);
  windowUtils.suspendTimeouts();
};

/**
 * Prepare to exit a nested event loop by enabling debuggee events.
 */
DeviceTabActor.prototype.postNest = function DTA_postNest(aNestData) {
  let windowUtils = this.browser
                        .QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIDOMWindowUtils);
  windowUtils.resumeTimeouts();
  windowUtils.suppressEventHandling(false);
};
