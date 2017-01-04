/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { utils: Cu, interfaces: Ci } = Components;

/* exported init */
this.EXPORTED_SYMBOLS = ["init"];

let gLoader;

function setupServer(mm) {
  // Prevent spawning multiple server per process, even if the caller call us
  // multiple times
  if (gLoader) {
    return gLoader;
  }

  // Lazy load Loader.jsm to prevent loading any devtools dependency too early.
  let { DevToolsLoader } =
    Cu.import("resource://devtools/shared/Loader.jsm", {});

  // Init a custom, invisible DebuggerServer, in order to not pollute the
  // debugger with all devtools modules, nor break the debugger itself with
  // using it in the same process.
  gLoader = new DevToolsLoader();
  gLoader.invisibleToDebugger = true;
  let { DebuggerServer } = gLoader.require("devtools/server/main");

  if (!DebuggerServer.initialized) {
    DebuggerServer.init();
  }
  // For browser content toolbox, we do need a regular root actor and all tab
  // actors, but don't need all the "browser actors" that are only useful when
  // debugging the parent process via the browser toolbox.
  DebuggerServer.registerActors({ browser: false, root: true, tab: true });

  // Clean up things when the client disconnects
  mm.addMessageListener("debug:content-process-destroy", function onDestroy() {
    mm.removeMessageListener("debug:content-process-destroy", onDestroy);

    DebuggerServer.destroy();
    gLoader.destroy();
    gLoader = null;
  });

  return gLoader;
}

function init(msg) {
  let mm = msg.target;
  mm.QueryInterface(Ci.nsISyncMessageSender);
  let prefix = msg.data.prefix;

  // Setup a server if none started yet
  let loader = setupServer(mm);

  // Connect both parent/child processes debugger servers RDP via message
  // managers
  let { DebuggerServer } = loader.require("devtools/server/main");
  let conn = DebuggerServer.connectToParent(prefix, mm);
  conn.parentMessageManager = mm;

  let { ChildProcessActor } =
    loader.require("devtools/server/actors/child-process");
  let { ActorPool } = loader.require("devtools/server/main");
  let actor = new ChildProcessActor(conn);
  let actorPool = new ActorPool(conn);
  actorPool.addActor(actor);
  conn.addActorPool(actorPool);

  let response = { actor: actor.form() };
  mm.sendAsyncMessage("debug:content-process-actor", response);
}
