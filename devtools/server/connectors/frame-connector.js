/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { dumpn } = DevToolsUtils;

loader.lazyRequireGetter(
  this,
  "DebuggerServer",
  "devtools/server/debugger-server",
  true
);
loader.lazyRequireGetter(
  this,
  "ChildDebuggerTransport",
  "devtools/shared/transport/child-transport",
  true
);

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

const FrameConnector = {
  /**
   * Start a DevTools server in a remote frame's process and add it as a child server for
   * an active connection.
   *
   * @param object connection
   *        The debugger server connection to use.
   * @param Element frame
   *        The frame element with remote content to connect to.
   * @param function [onDestroy]
   *        Optional function to invoke when the child process closes or the connection
   *        shuts down. (Need to forget about the related target actor.)
   * @return object
   *         A promise object that is resolved once the connection is established.
   */
  startServer(connection, frame, onDestroy, { addonId } = {}) {
    return new Promise(resolve => {
      // Get messageManager from XUL browser (which might be a specialized tunnel for RDM)
      // or else fallback to asking the frameLoader itself.
      let mm = frame.messageManager || frame.frameLoader.messageManager;
      mm.loadFrameScript("resource://devtools/server/startup/frame.js", false);

      const trackMessageManager = () => {
        frame.addEventListener("DevTools:BrowserSwap", onBrowserSwap);
        mm.addMessageListener("debug:setup-in-parent", onSetupInParent);
        mm.addMessageListener(
          "debug:spawn-actor-in-parent",
          onSpawnActorInParent
        );
        if (!actor) {
          mm.addMessageListener("debug:actor", onActorCreated);
        }
        DebuggerServer._childMessageManagers.add(mm);
      };

      const untrackMessageManager = () => {
        frame.removeEventListener("DevTools:BrowserSwap", onBrowserSwap);
        mm.removeMessageListener("debug:setup-in-parent", onSetupInParent);
        mm.removeMessageListener(
          "debug:spawn-actor-in-parent",
          onSpawnActorInParent
        );
        if (!actor) {
          mm.removeMessageListener("debug:actor", onActorCreated);
        }
        DebuggerServer._childMessageManagers.delete(mm);
      };

      let actor, childTransport;
      const prefix = connection.allocID("child");
      // Compute the same prefix that's used by DebuggerServerConnection
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
              `setupParent: '${setupParent}'\n${DevToolsUtils.safeErrorString(
                e
              )}`
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
          // in order to help identifying actor hiearchy via actor IDs.
          // Remove `/` as it may confuse message forwarding between processes.
          const contentPrefix = spawnedByActorID
            .replace(connection.prefix, "")
            .replace("/", "-");
          instance.actorID = connection.allocID(
            contentPrefix + "/" + instance.typeName
          );
          connection.addActor(instance);

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
              `constructor: '${constructor}'\n${DevToolsUtils.safeErrorString(
                e
              )}`
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
          onClosed() {},
        };
        childTransport.ready();

        connection.setForwarding(prefix, childTransport);

        dumpn(`Start forwarding for frame with prefix ${prefix}`);

        actor = msg.json.actor;
        resolve(actor);
      }).bind(this);

      // Listen for browser frame swap
      const onBrowserSwap = ({ detail: newFrame }) => {
        // Remove listeners from old frame and mm
        untrackMessageManager();
        // Update frame and mm to point to the new browser frame
        frame = newFrame;
        // Get messageManager from XUL browser (which might be a specialized tunnel for
        // RDM) or else fallback to asking the frameLoader itself.
        mm = frame.messageManager || frame.frameLoader.messageManager;
        // Add listeners to new frame and mm
        trackMessageManager();

        // provides hook to actor modules that need to exchange messages
        // between e10s parent and child processes
        parentModules.forEach(mod => {
          if (mod.onBrowserSwap) {
            mod.onBrowserSwap(mm);
          }
        });

        // Also notify actors spawned in the parent process about the new message manager.
        parentActors.forEach(parentActor => {
          if (parentActor.onBrowserSwap) {
            parentActor.onBrowserSwap(mm);
          }
        });

        if (childTransport) {
          childTransport.swapBrowser(mm);
        }
      };

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
        DebuggerServer.emit("disconnected-from-child:" + connPrefix, {
          mm,
          prefix: connPrefix,
        });

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
        if (actor) {
          // The FrameTargetActor within the child process doesn't necessary
          // have time to uninitialize itself when the frame is closed/killed.
          // So ensure telling the client that the related actor is detached.
          connection.send({ from: actor.actor, type: "tabDetached" });
          actor = null;
        }

        if (onDestroy) {
          onDestroy(mm);
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
  },
};

exports.FrameConnector = FrameConnector;
