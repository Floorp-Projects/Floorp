/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

try {

  var chromeGlobal = this;

// Encapsulate in its own scope to allows loading this frame script
// more than once.
  (function () {
    let Cu = Components.utils;
    let { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
    const DevToolsUtils = require("devtools/shared/DevToolsUtils");
    const { dumpn } = DevToolsUtils;
    const { DebuggerServer, ActorPool } = require("devtools/server/main");

  // Note that this frame script may be evaluated in non-e10s build
  // In such case, DebuggerServer is already going to be initialized.
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.isInChildProcess = true;
    }

  // In case of apps being loaded in parent process, DebuggerServer is already
  // initialized, but child specific actors are not registered.
  // Otherwise, for apps in child process, we need to load actors the first
  // time we load child.js
    DebuggerServer.addChildActors();

    let connections = new Map();

    let onConnect = DevToolsUtils.makeInfallible(function (msg) {
      removeMessageListener("debug:connect", onConnect);

      let mm = msg.target;
      let prefix = msg.data.prefix;

      let conn = DebuggerServer.connectToParent(prefix, mm);
      conn.parentMessageManager = mm;
      connections.set(prefix, conn);

      let actor = new DebuggerServer.ContentActor(conn, chromeGlobal, prefix);
      let actorPool = new ActorPool(conn);
      actorPool.addActor(actor);
      conn.addActorPool(actorPool);

      sendAsyncMessage("debug:actor", {actor: actor.form(), prefix: prefix});
    });

    addMessageListener("debug:connect", onConnect);

  // Allows executing module setup helper from the parent process.
  // See also: DebuggerServer.setupInChild()
    let onSetupInChild = DevToolsUtils.makeInfallible(msg => {
      let { module, setupChild, args } = msg.data;
      let m, fn;

      try {
        m = require(module);

        if (!setupChild in m) {
          dumpn("ERROR: module '" + module + "' does not export '" +
              setupChild + "'");
          return false;
        }

        m[setupChild].apply(m, args);

      } catch (e) {
        let error_msg = "exception during actor module setup running in the child process: ";
        DevToolsUtils.reportException(error_msg + e);
        dumpn("ERROR: " + error_msg + " \n\t module: '" + module +
            "' \n\t setupChild: '" + setupChild + "'\n" +
            DevToolsUtils.safeErrorString(e));
        return false;
      }
      if (msg.data.id) {
      // Send a message back to know when it is processed
        sendAsyncMessage("debug:setup-in-child-response",
                       {id: msg.data.id});
      }
      return true;
    });

    addMessageListener("debug:setup-in-child", onSetupInChild);

    let onDisconnect = DevToolsUtils.makeInfallible(function (msg) {
      removeMessageListener("debug:disconnect", onDisconnect);

    // Call DebuggerServerConnection.close to destroy all child actors
    // (It should end up calling DebuggerServerConnection.onClosed
    // that would actually cleanup all actor pools)
      let prefix = msg.data.prefix;
      let conn = connections.get(prefix);
      if (conn) {
        conn.close();
        connections.delete(prefix);
      }
    });
    addMessageListener("debug:disconnect", onDisconnect);

    let onInspect = DevToolsUtils.makeInfallible(function (msg) {
    // Store the node to be inspected in a global variable
    // (gInspectingNode). Later we'll fetch this variable again using
    // the findInspectingNode request over the remote debugging
    // protocol.
      let inspector = require("devtools/server/actors/inspector");
      inspector.setInspectingNode(msg.objects.node);
    });
    addMessageListener("debug:inspect", onInspect);
  })();

} catch (e) {
  dump("Exception in app child process: " + e + "\n");
}
