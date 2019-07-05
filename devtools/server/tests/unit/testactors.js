/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  LazyPool,
  createExtraActors,
} = require("devtools/shared/protocol/lazy-pool");
const { RootActor } = require("devtools/server/actors/root");
const { ThreadActor } = require("devtools/server/actors/thread");
const { DebuggerServer } = require("devtools/server/main");
const {
  ActorRegistry,
} = require("devtools/server/actors/utils/actor-registry");
const { TabSources } = require("devtools/server/actors/utils/TabSources");
const makeDebugger = require("devtools/server/actors/utils/make-debugger");

var gTestGlobals = new Set();
DebuggerServer.addTestGlobal = function(global) {
  gTestGlobals.add(global);
};
DebuggerServer.removeTestGlobal = function(global) {
  gTestGlobals.delete(global);
};

DebuggerServer.getTestGlobal = function(name) {
  for (const g of gTestGlobals) {
    if (g.__name == name) {
      return g;
    }
  }

  return null;
};

var gAllowNewThreadGlobals = false;
DebuggerServer.allowNewThreadGlobals = function() {
  gAllowNewThreadGlobals = true;
};
DebuggerServer.disallowNewThreadGlobals = function() {
  gAllowNewThreadGlobals = false;
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
  this._targetActorPool = new LazyPool(connection);

  for (const global of gTestGlobals) {
    const actor = new TestTargetActor(connection, global);
    actor.selected = false;
    this._targetActors.push(actor);
    this._targetActorPool.manage(actor);
  }
  if (this._targetActors.length > 0) {
    this._targetActors[0].selected = true;
  }
}

TestTabList.prototype = {
  constructor: TestTabList,
  destroy() {},
  getList: function() {
    return Promise.resolve([...this._targetActors]);
  },
};

exports.createRootActor = function createRootActor(connection) {
  ActorRegistry.registerModule("devtools/server/actors/webconsole", {
    prefix: "console",
    constructor: "WebConsoleActor",
    type: { target: true },
  });
  const root = new RootActor(connection, {
    tabList: new TestTabList(connection),
    globalActorFactories: ActorRegistry.globalActorFactories,
  });

  root.applicationType = "xpcshell-tests";
  return root;
};

function TestTargetActor(connection, global) {
  this.conn = connection;
  this._global = global;
  this._global.wrappedJSObject = global;
  this.threadActor = new ThreadActor(this, this._global);
  this.conn.addActor(this.threadActor);
  this._attached = false;
  this._extraActors = {};
  // This is a hack in order to enable threadActor to be accessed from getFront
  this._extraActors.contextActor = this.threadActor;
  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: () => [this._global],
    shouldAddNewGlobalAsDebuggee: g => {
      if (gAllowNewThreadGlobals) {
        return true;
      }

      return (
        g.hostAnnotations &&
        g.hostAnnotations.type == "document" &&
        g.hostAnnotations.element === this._global
      );
    },
  });
}

TestTargetActor.prototype = {
  constructor: TestTargetActor,
  actorPrefix: "TestTargetActor",

  get window() {
    return this._global;
  },

  get url() {
    return this._global.__name;
  },

  get sources() {
    if (!this._sources) {
      this._sources = new TabSources(this.threadActor);
    }
    return this._sources;
  },

  form: function() {
    const response = { actor: this.actorID, title: this._global.__name };

    // Walk over target-scoped actors and add them to a new LazyPool.
    const actorPool = new LazyPool(this.conn);
    const actors = createExtraActors(
      ActorRegistry.targetScopedActorFactories,
      actorPool,
      this
    );
    if (!actorPool.isEmpty()) {
      this._targetActorPool = actorPool;
      this.conn.addActorPool(this._targetActorPool);
    }

    return { ...response, ...actors };
  },

  onAttach: function(request) {
    this._attached = true;

    return { type: "tabAttached", threadActor: this.threadActor.actorID };
  },

  onDetach: function(request) {
    if (!this._attached) {
      return { error: "wrongState" };
    }
    return { type: "detached" };
  },

  onReload: function(request) {
    this.sources.reset();
    this.threadActor.clearDebuggees();
    this.threadActor.dbg.addDebuggees();
    return {};
  },

  removeActorByName: function(name) {
    const actor = this._extraActors[name];
    if (this._targetActorPool) {
      this._targetActorPool.removeActor(actor);
    }
    delete this._extraActors[name];
  },
};

TestTargetActor.prototype.requestTypes = {
  attach: TestTargetActor.prototype.onAttach,
  detach: TestTargetActor.prototype.onDetach,
  reload: TestTargetActor.prototype.onReload,
};
