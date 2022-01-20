/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { dumpn } = DevToolsUtils;
const { Pool } = require("devtools/shared/protocol/Pool");

loader.lazyRequireGetter(
  this,
  "DevToolsServer",
  "devtools/server/devtools-server",
  true
);
loader.lazyRequireGetter(
  this,
  "ChildDebuggerTransport",
  "devtools/shared/transport/child-transport",
  true
);

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

/**
 * Start a DevTools server in a remote frame's process and add it as a child server for
 * an active connection.
 *
 * @param object connection
 *        The devtools server connection to use.
 * @param Element frame
 *        The frame element with remote content to connect to.
 * @param function [onDestroy]
 *        Optional function to invoke when the child process closes or the connection
 *        shuts down. (Need to forget about the related target actor.)
 * @return object
 *         A promise object that is resolved once the connection is established.
 */
function connectToFrame(connection, frame, onDestroy, { addonId } = {}) {
  return new Promise(resolve => {
    // Get messageManager from XUL browser (which might be a specialized tunnel for RDM)
    // or else fallback to asking the frameLoader itself.
    const mm = frame.messageManager || frame.frameLoader.messageManager;
    mm.loadFrameScript("resource://devtools/server/startup/frame.js", false);

    const spawnInParentActorPool = new Pool(
      connection,
      "connectToFrame-spawnInParent"
    );
    connection.addActor(spawnInParentActorPool);

    const trackMessageManager = () => {
      mm.addMessageListener("debug:setup-in-parent", onSetupInParent);
      mm.addMessageListener(
        "debug:spawn-actor-in-parent",
        onSpawnActorInParent
      );
      if (!actor) {
        mm.addMessageListener("debug:actor", onActorCreated);
      }
    };

    const untrackMessageManager = () => {
      mm.removeMessageListener("debug:setup-in-parent", onSetupInParent);
      mm.removeMessageListener(
        "debug:spawn-actor-in-parent",
        onSpawnActorInParent
      );
      if (!actor) {
        mm.removeMessageListener("debug:actor", onActorCreated);
      }
    };

    let actor, childTransport;
    const prefix = connection.allocID("child");
    // Compute the same prefix that's used by DevToolsServerConnection
    const connPrefix = prefix + "/";

    // provides hook to actor modules that need to exchange messages
    // between e10s parent and child processes
    const parentModules = [];
    const onSetupInParent = function(msg) {
      // We may have multiple connectToFrame instance running for the same frame and
      // need to filter the messages.
      if (msg.json.prefix != connPrefix) {
        return false;
      }

      const { module, setupParent } = msg.json;
      let m;

      try {
        m = require(module);

        if (!(setupParent in m)) {
          dumpn(`ERROR: module '${module}' does not export '${setupParent}'`);
          return false;
        }

        parentModules.push(m[setupParent]({ mm, prefix: connPrefix }));

        return true;
      } catch (e) {
        const errorMessage =
          "Exception during actor module setup running in the parent process: ";
        DevToolsUtils.reportException(errorMessage + e);
        dumpn(
          `ERROR: ${errorMessage}\n\t module: '${module}'\n\t ` +
            `setupParent: '${setupParent}'\n${DevToolsUtils.safeErrorString(e)}`
        );
        return false;
      }
    };

    const parentActors = [];
    const onSpawnActorInParent = function(msg) {
      // We may have multiple connectToFrame instance running for the same tab
      // and need to filter the messages.
      if (msg.json.prefix != connPrefix) {
        return;
      }

      const { module, constructor, args, spawnedByActorID } = msg.json;
      let m;

      try {
        m = require(module);

        if (!(constructor in m)) {
          dump(`ERROR: module '${module}' does not export '${constructor}'`);
          return;
        }

        const Constructor = m[constructor];
        // Bind the actor to parent process connection so that these actors
        // directly communicates with the client as regular actors instanciated from
        // parent process
        const instance = new Constructor(connection, ...args, mm);
        instance.conn = connection;
        instance.parentID = spawnedByActorID;

        // Manually set the actor ID in order to insert parent actorID as prefix
        // in order to help identifying actor hierarchy via actor IDs.
        // Remove `/` as it may confuse message forwarding between processes.
        const contentPrefix = spawnedByActorID
          .replace(connection.prefix, "")
          .replace("/", "-");
        instance.actorID = connection.allocID(
          contentPrefix + "/" + instance.typeName
        );
        spawnInParentActorPool.manage(instance);

        mm.sendAsyncMessage("debug:spawn-actor-in-parent:actor", {
          prefix: connPrefix,
          actorID: instance.actorID,
        });

        parentActors.push(instance);
      } catch (e) {
        const errorMessage =
          "Exception during actor module setup running in the parent process: ";
        DevToolsUtils.reportException(errorMessage + e + "\n" + e.stack);
        dumpn(
          `ERROR: ${errorMessage}\n\t module: '${module}'\n\t ` +
            `constructor: '${constructor}'\n${DevToolsUtils.safeErrorString(e)}`
        );
      }
    };

    const onActorCreated = DevToolsUtils.makeInfallible(function(msg) {
      if (msg.json.prefix != prefix) {
        return;
      }
      mm.removeMessageListener("debug:actor", onActorCreated);

      // Pipe Debugger message from/to parent/child via the message manager
      childTransport = new ChildDebuggerTransport(mm, prefix);
      childTransport.hooks = {
        // Pipe all the messages from content process actors back to the client
        // through the parent process connection.
        onPacket: connection.send.bind(connection),
      };
      childTransport.ready();

      connection.setForwarding(prefix, childTransport);

      dumpn(`Start forwarding for frame with prefix ${prefix}`);

      actor = msg.json.actor;
      resolve(actor);
    });

    const destroy = DevToolsUtils.makeInfallible(function() {
      EventEmitter.off(connection, "closed", destroy);
      Services.obs.removeObserver(
        onMessageManagerClose,
        "message-manager-close"
      );

      // provides hook to actor modules that need to exchange messages
      // between e10s parent and child processes
      parentModules.forEach(mod => {
        if (mod.onDisconnected) {
          mod.onDisconnected();
        }
      });
      // TODO: Remove this deprecated path once it's no longer needed by add-ons.
      DevToolsServer.emit("disconnected-from-child:" + connPrefix, {
        mm,
        prefix: connPrefix,
      });

      // Destroy all actors created via spawnActorInParentProcess
      // in case content wasn't able to destroy them via a message
      spawnInParentActorPool.destroy();

      if (actor) {
        actor = null;
      }

      // Notify the tab descriptor about the destruction before the call to
      // `cancelForwarding`, so that we notify about the target destruction
      // *before* we purge all request for this prefix.
      // When we purge the requests, we also destroy all related fronts,
      // including the target front. This clears all event listeners
      // and ultimately prevent target-destroyed from firing.
      if (onDestroy) {
        onDestroy(mm);
      }

      if (childTransport) {
        // If we have a child transport, the actor has already
        // been created. We need to stop using this message manager.
        childTransport.close();
        childTransport = null;
        connection.cancelForwarding(prefix);

        // ... and notify the child process to clean the target-scoped actors.
        try {
          // Bug 1169643: Ignore any exception as the child process
          // may already be destroyed by now.
          mm.sendAsyncMessage("debug:disconnect", { prefix });
        } catch (e) {
          // Nothing to do
        }
      } else {
        // Otherwise, the frame has been closed before the actor
        // had a chance to be created, so we are not able to create
        // the actor.
        resolve(null);
      }

      // Cleanup all listeners
      untrackMessageManager();
    });

    // Listen for various messages and frame events
    trackMessageManager();

    // Listen for app process exit
    const onMessageManagerClose = function(subject, topic, data) {
      if (subject == mm) {
        destroy();
      }
    };
    Services.obs.addObserver(onMessageManagerClose, "message-manager-close");

    // Listen for connection close to cleanup things
    // when user unplug the device or we lose the connection somehow.
    EventEmitter.on(connection, "closed", destroy);

    mm.sendAsyncMessage("debug:connect", { prefix, addonId });
  });
}

exports.connectToFrame = connectToFrame;
