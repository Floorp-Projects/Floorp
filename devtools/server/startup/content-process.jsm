/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Module that listens for requests to start a `DebuggerServer` for an entire content
 * process.  Loaded into content processes by the main process during
 * content-process-connector.js' `connectToContentProcess` via the process
 * script `content-process.js`.
 *
 * The actual server startup itself is in this JSM so that code can be cached.
 */

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
  const { DevToolsLoader } = ChromeUtils.import(
    "resource://devtools/shared/Loader.jsm"
  );

  // Init a custom, invisible DebuggerServer, in order to not pollute the
  // debugger with all devtools modules, nor break the debugger itself with
  // using it in the same process.
  gLoader = new DevToolsLoader({
    invisibleToDebugger: true,
  });
  const { DebuggerServer } = gLoader.require("devtools/server/debugger-server");

  DebuggerServer.init();
  // For browser content toolbox, we do need a regular root actor and all tab
  // actors, but don't need all the "browser actors" that are only useful when
  // debugging the parent process via the browser toolbox.
  DebuggerServer.registerActors({ root: true, target: true });

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
  const mm = msg.target;
  const prefix = msg.data.prefix;

  // Setup a server if none started yet
  const loader = setupServer(mm);

  // Connect both parent/child processes debugger servers RDP via message
  // managers
  const { DebuggerServer } = loader.require("devtools/server/debugger-server");
  const conn = DebuggerServer.connectToParent(prefix, mm);
  conn.parentMessageManager = mm;

  const { ContentProcessTargetActor } = loader.require(
    "devtools/server/actors/targets/content-process"
  );
  const { ActorPool } = loader.require("devtools/server/actors/common");
  const actor = new ContentProcessTargetActor(conn);
  const actorPool = new ActorPool(conn);
  actorPool.addActor(actor);
  conn.addActorPool(actorPool);

  const response = { actor: actor.form() };
  mm.sendAsyncMessage("debug:content-process-actor", response);
}
