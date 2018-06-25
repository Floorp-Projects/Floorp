/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global addEventListener, addMessageListener, removeMessageListener, sendAsyncMessage */

/*
 * Frame script that listens for requests to start a `DebuggerServer` for a frame in a
 * content process.  Loaded into content process frames by the main process during
 * `DebuggerServer.connectToFrame`.
 */

try {
  var chromeGlobal = this;

  // Encapsulate in its own scope to allows loading this frame script more than once.
  (function() {
    const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});

    const DevToolsUtils = require("devtools/shared/DevToolsUtils");
    const { dumpn } = DevToolsUtils;
    const { DebuggerServer, ActorPool } = require("devtools/server/main");

    DebuggerServer.init();
    // We want a special server without any root actor and only target-scoped actors.
    // We are going to spawn a FrameTargetActor instance in the next few lines,
    // it is going to act like a root actor without being one.
    DebuggerServer.registerActors({ target: true });

    const connections = new Map();

    const onConnect = DevToolsUtils.makeInfallible(function(msg) {
      removeMessageListener("debug:connect", onConnect);

      const mm = msg.target;
      const prefix = msg.data.prefix;
      const addonId = msg.data.addonId;

      // Using the JS debugger causes problems when we're trying to
      // schedule those zone groups across different threads. Calling
      // blockThreadedExecution causes Gecko to switch to a simpler
      // single-threaded model until unblockThreadedExecution is
      // called later. We cannot start the debugger until the callback
      // passed to blockThreadedExecution has run, signaling that
      // we're running single-threaded.
      Cu.blockThreadedExecution(() => {
        const conn = DebuggerServer.connectToParent(prefix, mm);
        conn.parentMessageManager = mm;
        connections.set(prefix, conn);

        let actor;

        if (addonId) {
          const { WebExtensionTargetActor } = require("devtools/server/actors/targets/webextension");
          actor = new WebExtensionTargetActor(conn, chromeGlobal, prefix, addonId);
        } else {
          const { FrameTargetActor } = require("devtools/server/actors/targets/frame");
          actor = new FrameTargetActor(conn, chromeGlobal);
        }

        const actorPool = new ActorPool(conn);
        actorPool.addActor(actor);
        conn.addActorPool(actorPool);

        sendAsyncMessage("debug:actor", {actor: actor.form(), prefix: prefix});
      });
    });

    addMessageListener("debug:connect", onConnect);

    // Allows executing module setup helper from the parent process.
    // See also: DebuggerServer.setupInChild()
    const onSetupInChild = DevToolsUtils.makeInfallible(msg => {
      const { module, setupChild, args } = msg.data;
      let m;

      try {
        m = require(module);

        if (!(setupChild in m)) {
          dumpn(`ERROR: module '${module}' does not export '${setupChild}'`);
          return false;
        }

        m[setupChild].apply(m, args);
      } catch (e) {
        const errorMessage =
          "Exception during actor module setup running in the child process: ";
        DevToolsUtils.reportException(errorMessage + e);
        dumpn(`ERROR: ${errorMessage}\n\t module: '${module}'\n\t ` +
              `setupChild: '${setupChild}'\n${DevToolsUtils.safeErrorString(e)}`);
        return false;
      }
      if (msg.data.id) {
        // Send a message back to know when it is processed
        sendAsyncMessage("debug:setup-in-child-response", {id: msg.data.id});
      }
      return true;
    });

    addMessageListener("debug:setup-in-child", onSetupInChild);

    const onDisconnect = DevToolsUtils.makeInfallible(function(msg) {
      const prefix = msg.data.prefix;
      const conn = connections.get(prefix);
      if (!conn) {
        // Several copies of this frame script can be running for a single frame since it
        // is loaded once for each DevTools connection to the frame.  If this disconnect
        // request doesn't match a connection known here, ignore it.
        return;
      }

      Cu.unblockThreadedExecution();

      removeMessageListener("debug:disconnect", onDisconnect);
      // Call DebuggerServerConnection.close to destroy all child actors. It should end up
      // calling DebuggerServerConnection.onClosed that would actually cleanup all actor
      // pools.
      conn.close();
      connections.delete(prefix);
    });
    addMessageListener("debug:disconnect", onDisconnect);

    // In non-e10s mode, the "debug:disconnect" message isn't always received before the
    // messageManager connection goes away.  Watching for "unload" here ensures we close
    // any connections when the frame is unloaded.
    addEventListener("unload", () => {
      for (const conn of connections.values()) {
        conn.close();
      }
      connections.clear();
    });
  })();
} catch (e) {
  dump(`Exception in DevTools frame startup: ${e}\n`);
}
