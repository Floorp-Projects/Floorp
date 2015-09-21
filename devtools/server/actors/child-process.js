/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");

const { ChromeDebuggerActor } = require("devtools/server/actors/script");
const { WebConsoleActor } = require("devtools/server/actors/webconsole");
const makeDebugger = require("devtools/server/actors/utils/make-debugger");
const { ActorPool } = require("devtools/server/main");
const Services = require("Services");
const { dbg_assert } = require("devtools/shared/DevToolsUtils");
const { TabSources } = require("./utils/TabSources");

function ChildProcessActor(aConnection) {
  this.conn = aConnection;
  this._contextPool = new ActorPool(this.conn);
  this.conn.addActorPool(this._contextPool);
  this.threadActor = null;

  // Use a see-everything debugger
  this.makeDebugger = makeDebugger.bind(null, {
    findDebuggees: dbg => dbg.findAllGlobals(),
    shouldAddNewGlobalAsDebuggee: global => true
  });

  // Scope into which the webconsole executes:
  // An empty sandbox with chrome privileges
  let systemPrincipal = Cc["@mozilla.org/systemprincipal;1"]
    .createInstance(Ci.nsIPrincipal);
  let sandbox = Cu.Sandbox(systemPrincipal);
  this._consoleScope = sandbox;
}
exports.ChildProcessActor = ChildProcessActor;

ChildProcessActor.prototype = {
  actorPrefix: "process",

  get isRootActor() {
    return true;
  },

  get exited() {
    return !this._contextPool;
  },

  get url() {
    return undefined;
  },

  get window() {
    return this._consoleScope;
  },

  get sources() {
    if (!this._sources) {
      dbg_assert(this.threadActor, "threadActor should exist when creating sources.");
      this._sources = new TabSources(this.threadActor);
    }
    return this._sources;
  },

  form: function() {
    if (!this._consoleActor) {
      this._consoleActor = new WebConsoleActor(this.conn, this);
      this._contextPool.addActor(this._consoleActor);
    }

    if (!this.threadActor) {
      this.threadActor = new ChromeDebuggerActor(this.conn, this);
      this._contextPool.addActor(this.threadActor);
    }

    return {
      actor: this.actorID,
      name: "Content process",

      consoleActor: this._consoleActor.actorID,
      chromeDebugger: this.threadActor.actorID,

      traits: {
        highlightable: false,
        networkMonitor: false,
      },
    };
  },

  disconnect: function() {
    this.conn.removeActorPool(this._contextPool);
    this._contextPool = null;
  },

  preNest: function() {
    // TODO: freeze windows
    // window mediator doesn't work in child.
    // it doesn't throw, but doesn't return any window
  },

  postNest: function() {
  },
};

ChildProcessActor.prototype.requestTypes = {
};
