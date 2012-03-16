/* -*- Mode: javascript; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

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
  this.conn = connection;
  this._tabActors = new WeakMap();
  this._tabActorPool = null;
  this._actorFactories = null;
  this.browser = Services.wm.getMostRecentWindow('navigator:browser');
}

DeviceRootActor.prototype = {
  /**
   * Return a 'hello' packet as specified by the Remote Debugging Protocol.
   */
  sayHello: function DRA_sayHello() {
    return {
      from: 'root',
      applicationType: 'browser',
      traits: []
    };
  },

  /**
   * Disconnects the actor from the browser window.
   */
  disconnect: function DRA_disconnect() {
    let actor = this._tabActors.get(this.browser);
    if (actor) {
      actor.exit();
    }
  },

  /**
   * Handles the listTabs request.  Builds a list of actors for the single
   * tab (window) running in the process. The actors will survive
   * until at least the next listTabs request.
   */
  onListTabs: function DRA_onListTabs() {
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
  }

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
  this.conn = connection;
  this._browser = browser;
}

DeviceTabActor.prototype = {
  get browser() {
    return this._browser;
  },

  get exited() {
    return !this.browser;
  },

  get attached() {
    return !!this._attached
  },

  _tabPool: null,
  get tabActorPool() {
    return this._tabPool;
  },

  _contextPool: null,
  get contextActorPool() {
    return this._contextPool;
  },

  actorPrefix: 'tab',

  grip: function DTA_grip() {
    dbg_assert(!this.exited,
               'grip() should not be called on exited browser actor.');
    dbg_assert(this.actorID,
               'tab should have an actorID.');
    return {
      'actor': this.actorID,
      'title': this.browser.contentTitle,
      'url': this.browser.document.documentURI
    }
  },

  /**
   * Called when the actor is removed from the connection.
   */
  disconnect: function DTA_disconnect() {
    this._detach();
  },

  /**
   * Called by the root actor when the underlying tab is closed.
   */
  exit: function DTA_exit() {
    if (this.exited) {
      return;
    }

    if (this.attached) {
      this._detach();
      this.conn.send({
        'from': this.actorID,
        'type': 'tabDetached'
      });
    }

    this._browser = null;
  },

  /**
   * Does the actual work of attaching to a tab.
   */
  _attach: function DTA_attach() {
    if (this._attached) {
      return;
    }

    // Create a pool for tab-lifetime actors.
    dbg_assert(!this._tabPool, 'Should not have a tab pool if we were not attached.');
    this._tabPool = new ActorPool(this.conn);
    this.conn.addActorPool(this._tabPool);

    // ... and a pool for context-lifetime actors.
    this._pushContext();

    this._attached = true;
  },

  /**
   * Creates a thread actor and a pool for context-lifetime actors. It then sets
   * up the content window for debugging.
   */
  _pushContext: function DTA_pushContext() {
    dbg_assert(!this._contextPool, "Can't push multiple contexts");

    this._contextPool = new ActorPool(this.conn);
    this.conn.addActorPool(this._contextPool);

    this.threadActor = new ThreadActor(this);
    this._addDebuggees(this.browser.content.wrappedJSObject);
    this._contextPool.addActor(this.threadActor);
  },

  /**
   * Add the provided window and all windows in its frame tree as debuggees.
   */
  _addDebuggees: function DTA__addDebuggees(content) {
    this.threadActor.addDebuggee(content);
    let frames = content.frames;
    for (let i = 0; i < frames.length; i++) {
      this._addDebuggees(frames[i]);
    }
  },

  /**
   * Exits the current thread actor and removes the context-lifetime actor pool.
   * The content window is no longer being debugged after this call.
   */
  _popContext: function DTA_popContext() {
    dbg_assert(!!this._contextPool, 'No context to pop.');

    this.conn.removeActorPool(this._contextPool);
    this._contextPool = null;
    this.threadActor.exit();
    this.threadActor = null;
  },

  /**
   * Does the actual work of detaching from a tab.
   */
  _detach: function DTA_detach() {
    if (!this.attached) {
      return;
    }

    this._popContext();

    // Shut down actors that belong to this tab's pool.
    this.conn.removeActorPool(this._tabPool);
    this._tabPool = null;

    this._attached = false;
  },

  // Protocol Request Handlers

  onAttach: function DTA_onAttach(aRequest) {
    if (this.exited) {
      return { type: 'exited' };
    }

    this._attach();

    return { type: 'tabAttached', threadActor: this.threadActor.actorID };
  },

  onDetach: function DTA_onDetach(aRequest) {
    if (!this.attached) {
      return { error: 'wrongState' };
    }

    this._detach();

    return { type: 'detached' };
  },

  /**
   * Prepare to enter a nested event loop by disabling debuggee events.
   */
  preNest: function DTA_preNest() {
    let windowUtils = this.browser.content
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.suppressEventHandling(true);
    windowUtils.suspendTimeouts();
  },

  /**
   * Prepare to exit a nested event loop by enabling debuggee events.
   */
  postNest: function DTA_postNest(aNestData) {
    let windowUtils = this.browser.content
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDOMWindowUtils);
    windowUtils.resumeTimeouts();
    windowUtils.suppressEventHandling(false);
  }

};

/**
 * The request types this actor can handle.
 */
DeviceTabActor.prototype.requestTypes = {
  'attach': DeviceTabActor.prototype.onAttach,
  'detach': DeviceTabActor.prototype.onDetach
};
