/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global content, addEventListener, addMessageListener, removeMessageListener,
  sendAsyncMessage */

/*
 * Frame script that listens for requests to start a `DevToolsServer` for a frame in a
 * content process.  Loaded into content process frames by the main process during
 * frame-connector.js' connectToFrame.
 */

try {
  var chromeGlobal = this;

  // Encapsulate in its own scope to allows loading this frame script more than once.
  (function() {
    // In most cases, we are debugging a tab in content process, without chrome
    // privileges. But in some tests, we are attaching to privileged document.
    // Because the debugger can't be running in the same compartment than its debuggee,
    // we have to load the server in a dedicated Loader, flagged with
    // invisibleToDebugger, which will force it to be loaded in another compartment.
    let loader,
      customLoader = false;
    if (content.document.nodePrincipal.isSystemPrincipal) {
      const { DevToolsLoader } = ChromeUtils.import(
        "resource://devtools/shared/Loader.jsm"
      );
      loader = new DevToolsLoader({
        invisibleToDebugger: true,
      });
      customLoader = true;
    } else {
      // Otherwise, use the shared loader.
      loader = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
    }
    const { require } = loader;

    const DevToolsUtils = require("devtools/shared/DevToolsUtils");
    const { dumpn } = DevToolsUtils;
    const { DevToolsServer } = require("devtools/server/devtools-server");
    const { ActorPool } = require("devtools/server/actors/common");

    DevToolsServer.init();
    // We want a special server without any root actor and only target-scoped actors.
    // We are going to spawn a FrameTargetActor instance in the next few lines,
    // it is going to act like a root actor without being one.
    DevToolsServer.registerActors({ target: true });

    const connections = new Map();

    const onConnect = DevToolsUtils.makeInfallible(function(msg) {
      removeMessageListener("debug:connect", onConnect);

      const mm = msg.target;
      const prefix = msg.data.prefix;
      const addonId = msg.data.addonId;

      const conn = DevToolsServer.connectToParent(prefix, mm);
      conn.parentMessageManager = mm;
      connections.set(prefix, conn);

      let actor;

      if (addonId) {
        const {
          WebExtensionTargetActor,
        } = require("devtools/server/actors/targets/webextension");
        actor = new WebExtensionTargetActor(
          conn,
          chromeGlobal,
          prefix,
          addonId
        );
      } else {
        const {
          FrameTargetActor,
        } = require("devtools/server/actors/targets/frame");
        const { docShell } = chromeGlobal;
        // For a script loaded via loadFrameScript, the global is the content
        // message manager.
        actor = new FrameTargetActor(conn, docShell);
      }

      const actorPool = new ActorPool(conn, "frame-script");
      actorPool.addActor(actor);
      conn.addActorPool(actorPool);

      sendAsyncMessage("debug:actor", { actor: actor.form(), prefix: prefix });
    });

    addMessageListener("debug:connect", onConnect);

    // Allows executing module setup helper from the parent process.
    // See also: DevToolsServer.setupInChild()
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
        dumpn(
          `ERROR: ${errorMessage}\n\t module: '${module}'\n\t ` +
            `setupChild: '${setupChild}'\n${DevToolsUtils.safeErrorString(e)}`
        );
        return false;
      }
      if (msg.data.id) {
        // Send a message back to know when it is processed
        sendAsyncMessage("debug:setup-in-child-response", { id: msg.data.id });
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

      removeMessageListener("debug:disconnect", onDisconnect);
      // Call DevToolsServerConnection.close to destroy all child actors. It should end up
      // calling DevToolsServerConnection.onClosed that would actually cleanup all actor
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

    // Destroy the server once its last connection closes. Note that multiple frame
    // scripts may be running in parallel and reuse the same server.
    function destroyServer() {
      // Only destroy the server if there is no more connections to it. It may be used
      // to debug another tab running in the same process.
      if (DevToolsServer.hasConnection() || DevToolsServer.keepAlive) {
        return;
      }
      DevToolsServer.off("connectionchange", destroyServer);
      DevToolsServer.destroy();

      // When debugging chrome pages, we initialized a dedicated loader, also destroy it
      if (customLoader) {
        loader.destroy();
      }
    }
    DevToolsServer.on("connectionchange", destroyServer);
  })();
} catch (e) {
  dump(`Exception in DevTools frame startup: ${e}\n`);
}
