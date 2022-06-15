/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  LazyPool,
  createExtraActors,
} = require("devtools/shared/protocol/lazy-pool");
const { RootActor } = require("devtools/server/actors/root");
const { ThreadActor } = require("devtools/server/actors/thread");
const { DevToolsServer } = require("devtools/server/devtools-server");
const {
  ActorRegistry,
} = require("devtools/server/actors/utils/actor-registry");
const {
  SourcesManager,
} = require("devtools/server/actors/utils/sources-manager");
const makeDebugger = require("devtools/server/actors/utils/make-debugger");
const protocol = require("devtools/shared/protocol");
const {
  windowGlobalTargetSpec,
} = require("devtools/shared/specs/targets/window-global");
const { tabDescriptorSpec } = require("devtools/shared/specs/descriptors/tab");
const Targets = require("devtools/server/actors/targets/index");
const {
  createContentProcessSessionContext,
} = require("devtools/server/actors/watcher/session-context");

var gTestGlobals = new Set();
DevToolsServer.addTestGlobal = function(global) {
  gTestGlobals.add(global);
};
DevToolsServer.removeTestGlobal = function(global) {
  gTestGlobals.delete(global);
};

DevToolsServer.getTestGlobal = function(name) {
  for (const g of gTestGlobals) {
    if (g.__name == name) {
      return g;
    }
  }

  return null;
};

var gAllowNewThreadGlobals = false;
DevToolsServer.allowNewThreadGlobals = function() {
  gAllowNewThreadGlobals = true;
};
DevToolsServer.disallowNewThreadGlobals = function() {
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
  // DevToolsServer.addTestGlobal.
  this._descriptorActors = [];

  // A pool mapping those actors' names to the actors.
  this._descriptorActorPool = new LazyPool(connection);

  for (const global of gTestGlobals) {
    const actor = new TestTargetActor(connection, global);
    this._descriptorActorPool.manage(actor);

    const descriptorActor = new TestDescriptorActor(connection, actor);
    this._descriptorActorPool.manage(descriptorActor);

    this._descriptorActors.push(descriptorActor);
  }
}

TestTabList.prototype = {
  constructor: TestTabList,
  destroy() {},
  getList: function() {
    return Promise.resolve([...this._descriptorActors]);
  },
  // Helper method only available for the xpcshell implementation of tablist.
  getTargetActorForTab: function(title) {
    const descriptorActor = this._descriptorActors.find(d => d.title === title);
    if (!descriptorActor) {
      return null;
    }
    return descriptorActor._targetActor;
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

const TestDescriptorActor = protocol.ActorClassWithSpec(tabDescriptorSpec, {
  initialize: function(conn, targetActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.conn = conn;
    this._targetActor = targetActor;
  },

  // We don't exercise the selected tab in xpcshell tests.
  get selected() {
    return false;
  },

  get title() {
    return this._targetActor.title;
  },

  form() {
    const form = {
      actor: this.actorID,
      traits: {},
      selected: this.selected,
      title: this._targetActor.title,
      url: this._targetActor.url,
    };

    return form;
  },

  getFavicon() {
    return "";
  },

  getTarget() {
    return this._targetActor.form();
  },
});

const TestTargetActor = protocol.ActorClassWithSpec(windowGlobalTargetSpec, {
  initialize: function(conn, global) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.conn = conn;

    this.sessionContext = createContentProcessSessionContext();
    this._global = global;
    this._global.wrappedJSObject = global;
    this.threadActor = new ThreadActor(this, this._global);
    this.conn.addActor(this.threadActor);
    this._extraActors = {};
    // This is a hack in order to enable threadActor to be accessed from getFront
    this._extraActors.threadActor = this.threadActor;
    this.makeDebugger = makeDebugger.bind(null, {
      findDebuggees: () => [this._global],
      shouldAddNewGlobalAsDebuggee: g => gAllowNewThreadGlobals,
    });
    this.dbg = this.makeDebugger();
    this.notifyResourceAvailable = this.notifyResourceAvailable.bind(this);
  },

  targetType: Targets.TYPES.FRAME,

  get window() {
    return this._global;
  },

  // Both title and url point to this._global.__name
  get title() {
    return this._global.__name;
  },

  get url() {
    return this._global.__name;
  },

  get sourcesManager() {
    if (!this._sourcesManager) {
      this._sourcesManager = new SourcesManager(this.threadActor);
    }
    return this._sourcesManager;
  },

  form: function() {
    const response = {
      actor: this.actorID,
      title: this.title,
      threadActor: this.threadActor.actorID,
    };

    // Walk over target-scoped actors and add them to a new LazyPool.
    const actorPool = new LazyPool(this.conn);
    const actors = createExtraActors(
      ActorRegistry.targetScopedActorFactories,
      actorPool,
      this
    );
    if (actorPool?._poolMap.size > 0) {
      this._descriptorActorPool = actorPool;
      this.conn.addActorPool(this._descriptorActorPool);
    }

    return { ...response, ...actors };
  },

  detach: function(request) {
    this.threadActor.destroy();
    return { type: "detached" };
  },

  reload: function(request) {
    this.sourcesManager.reset();
    this.threadActor.clearDebuggees();
    this.threadActor.dbg.addDebuggees();
    return {};
  },

  removeActorByName: function(name) {
    const actor = this._extraActors[name];
    if (this._descriptorActorPool) {
      this._descriptorActorPool.removeActor(actor);
    }
    delete this._extraActors[name];
  },

  notifyResourceAvailable(resources) {
    this.emit("resource-available-form", resources);
  },
});
