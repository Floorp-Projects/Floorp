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
  this._extraActors = null;
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
  let actorPool = new ActorPool(this.conn);

#ifndef MOZ_WIDGET_GONK
  let actor = this._tabActors.get(this.browser);
  if (!actor) {
    actor = new DeviceTabActor(this.conn, this.browser);
    // this.actorID is set by ActorPool when an actor is put into one.
    actor.parentID = this.actorID;
    this._tabActors.set(this.browser, actor);
  }
  actorPool.addActor(actor);
#endif

  this._createExtraActors(DebuggerServer.globalActorFactories, actorPool);

  // Now drop the old actorID -> actor map. Actors that still mattered were
  // added to the new map, others will go away.
  if (this._tabActorPool) {
    this.conn.removeActorPool(this._tabActorPool);
  }
  this._tabActorPool = actorPool;
  this.conn.addActorPool(this._tabActorPool);

  let response = {
    'from': 'root',
    'selected': 0,
#ifndef MOZ_WIDGET_GONK
    'tabs': [actor.grip()]
#else
    'tabs': []
#endif
  };
  this._appendExtraActors(response);
  return response;
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

Object.defineProperty(DeviceTabActor.prototype, "title", {
  get: function() {
    return this.browser.title;
  },
  enumerable: true,
  configurable: false
});

Object.defineProperty(DeviceTabActor.prototype, "url", {
  get: function() {
    return this.browser.document.documentURI;
  },
  enumerable: true,
  configurable: false
});

Object.defineProperty(DeviceTabActor.prototype, "contentWindow", {
  get: function() {
    return this.browser;
  },
  enumerable: true,
  configurable: false
});
