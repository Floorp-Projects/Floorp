/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Module that listens for requests to start a `DevToolsServer` for an entire content
 * process.  Loaded into content processes by the main process during
 * content-process-connector.js' `connectToContentProcess` via the process
 * script `content-process.js`.
 *
 * The actual server startup itself is in this JSM so that code can be cached.
 */

/* exported init */
const EXPORTED_SYMBOLS = ["init"];

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

  // Init a custom, invisible DevToolsServer, in order to not pollute the
  // debugger with all devtools modules, nor break the debugger itself with
  // using it in the same process.
  gLoader = new DevToolsLoader({
    invisibleToDebugger: true,
  });
  const { DevToolsServer } = gLoader.require("devtools/server/devtools-server");

  DevToolsServer.init();
  // For browser content toolbox, we do need a regular root actor and all tab
  // actors, but don't need all the "browser actors" that are only useful when
  // debugging the parent process via the browser toolbox.
  DevToolsServer.registerActors({ root: true, target: true });

  // Destroy the server once its last connection closes. Note that multiple frame
  // scripts may be running in parallel and reuse the same server.
  function destroyServer() {
    // Only destroy the server if there is no more connections to it. It may be used
    // to debug the same process from another client.
    if (DevToolsServer.hasConnection()) {
      return;
    }
    DevToolsServer.off("connectionchange", destroyServer);

    DevToolsServer.destroy();
    gLoader.destroy();
    gLoader = null;
  }
  DevToolsServer.on("connectionchange", destroyServer);

  return gLoader;
}

function init(msg) {
  const mm = msg.target;
  const prefix = msg.data.prefix;

  // Setup a server if none started yet
  const loader = setupServer(mm);

  // Connect both parent/child processes devtools servers RDP via message
  // managers
  const { DevToolsServer } = loader.require("devtools/server/devtools-server");
  const conn = DevToolsServer.connectToParent(prefix, mm);
  conn.parentMessageManager = mm;

  const { ContentProcessTargetActor } = loader.require(
    "devtools/server/actors/targets/content-process"
  );
  const { ActorPool } = loader.require("devtools/server/actors/common");
  const actor = new ContentProcessTargetActor(conn);
  const actorPool = new ActorPool(conn, "content-process");
  actorPool.addActor(actor);
  conn.addActorPool(actorPool);

  const response = { actor: actor.form() };
  mm.sendAsyncMessage("debug:content-process-actor", response);

  // Clean up things when the client disconnects
  mm.addMessageListener("debug:content-process-disconnect", function onDestroy(
    message
  ) {
    if (message.data.prefix != prefix) {
      // Several copies of this process script can be running for a single process if
      // we are debugging the same process from multiple clients.
      // If this disconnect request doesn't match a connection known here, ignore it.
      return;
    }
    mm.removeMessageListener("debug:content-process-disconnect", onDestroy);

    // Call DevToolsServerConnection.close to destroy all child actors. It should end up
    // calling DevToolsServerConnection.onClosed that would actually cleanup all actor
    // pools.
    conn.close();
  });
}
