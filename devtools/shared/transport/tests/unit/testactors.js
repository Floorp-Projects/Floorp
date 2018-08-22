/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { ActorPool, appendExtraActors, createExtraActors } =
  require("devtools/server/actors/common");
const { RootActor } = require("devtools/server/actors/root");
const { ThreadActor } = require("devtools/server/actors/thread");
const { DebuggerServer } = require("devtools/server/main");
const promise = require("promise");

var gTestGlobals = [];
DebuggerServer.addTestGlobal = function(global) {
  gTestGlobals.push(global);
};

// A mock tab list, for use by tests. This simply presents each global in
// gTestGlobals as a tab, and the list is fixed: it never calls its
// onListChanged handler.
//
// As implemented now, we consult gTestGlobals when we're constructed, not
// when we're iterated over, so tests have to add their globals before the
// root actor is created.
function TestTabList(connection) {
  this.conn = connection;

  // An array of actors for each global added with
  // DebuggerServer.addTestGlobal.
  this._targetActors = [];

  // A pool mapping those actors' names to the actors.
  this._targetActorPool = new ActorPool(connection);

  for (const global of gTestGlobals) {
    const actor = new TestTargetActor(connection, global);
    actor.selected = false;
    this._targetActors.push(actor);
    this._targetActorPool.addActor(actor);
  }
  if (this._targetActors.length > 0) {
    this._targetActors[0].selected = true;
  }

  connection.addActorPool(this._targetActorPool);
}

TestTabList.prototype = {
  constructor: TestTabList,
  getList: function() {
    return promise.resolve([...this._targetActors]);
  }
};

exports.createRootActor = function createRootActor(connection) {
  const root = new RootActor(connection, {
    tabList: new TestTabList(connection),
    globalActorFactories: DebuggerServer.globalActorFactories
  });
  root.applicationType = "xpcshell-tests";
  return root;
};

function TestTargetActor(connection, global) {
  this.conn = connection;
  this._global = global;
  this._threadActor = new ThreadActor(this, this._global);
  this.conn.addActor(this._threadActor);
  this._attached = false;
  this._extraActors = {};
}

TestTargetActor.prototype = {
  constructor: TestTargetActor,
  actorPrefix: "TestTargetActor",

  get window() {
    return { wrappedJSObject: this._global };
  },

  get url() {
    return this._global.__name;
  },

  form: function() {
    const response = { actor: this.actorID, title: this._global.__name };

    // Walk over target-scoped actors and add them to a new ActorPool.
    const actorPool = new ActorPool(this.conn);
    this._createExtraActors(DebuggerServer.targetScopedActorFactories, actorPool);
    if (!actorPool.isEmpty()) {
      this._targetActorPool = actorPool;
      this.conn.addActorPool(this._targetActorPool);
    }

    this._appendExtraActors(response);

    return response;
  },

  onAttach: function(request) {
    this._attached = true;

    const response = { type: "tabAttached", threadActor: this._threadActor.actorID };
    this._appendExtraActors(response);

    return response;
  },

  onDetach: function(request) {
    if (!this._attached) {
      return { "error": "wrongState" };
    }
    return { type: "detached" };
  },

  /* Support for DebuggerServer.addTargetScopedActor. */
  _createExtraActors: createExtraActors,
  _appendExtraActors: appendExtraActors
};

TestTargetActor.prototype.requestTypes = {
  "attach": TestTargetActor.prototype.onAttach,
  "detach": TestTargetActor.prototype.onDetach
};
