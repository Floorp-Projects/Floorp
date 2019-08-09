/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Services = require("Services");
var DevToolsUtils = require("devtools/shared/DevToolsUtils");
var { dumpn } = DevToolsUtils;

loader.lazyRequireGetter(
  this,
  "ChildDebuggerTransport",
  "devtools/shared/transport/child-transport",
  true
);

const CONTENT_PROCESS_SERVER_STARTUP_SCRIPT =
  "resource://devtools/server/startup/content-process.js";

loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

const ContentProcessConnector = {
  // Flag to check if the content process server startup script was already loaded.
  _contentProcessServerStartupScriptLoaded: false,

  /**
   * Start a DevTools server in a content process (representing the entire process, not
   * just a single frame) and add it as a child server for an active connection.
   */
  startServer(connection, mm, onDestroy) {
    return new Promise(resolve => {
      const prefix = connection.allocID("content-process");
      let actor, childTransport;

      mm.addMessageListener("debug:content-process-actor", function listener(
        msg
      ) {
        // Arbitrarily choose the first content process to reply
        // XXX: This code needs to be updated if we use more than one content process
        mm.removeMessageListener("debug:content-process-actor", listener);

        // Pipe Debugger message from/to parent/child via the message manager
        childTransport = new ChildDebuggerTransport(mm, prefix);
        childTransport.hooks = {
          onPacket: connection.send.bind(connection),
          onClosed() {},
        };
        childTransport.ready();

        connection.setForwarding(prefix, childTransport);

        dumpn(`Start forwarding for process with prefix ${prefix}`);

        actor = msg.json.actor;

        resolve(actor);
      });

      // Load the content process server startup script only once.
      if (!this._contentProcessServerStartupScriptLoaded) {
        // Load the process script that will receive the debug:init-content-server message
        Services.ppmm.loadProcessScript(
          CONTENT_PROCESS_SERVER_STARTUP_SCRIPT,
          true
        );
        this._contentProcessServerStartupScriptLoaded = true;
      }

      // Send a message to the content process server startup script to forward it the
      // prefix.
      mm.sendAsyncMessage("debug:init-content-server", {
        prefix: prefix,
      });

      function onClose() {
        Services.obs.removeObserver(
          onMessageManagerClose,
          "message-manager-close"
        );
        EventEmitter.off(connection, "closed", onClose);
        if (childTransport) {
          // If we have a child transport, the actor has already
          // been created. We need to stop using this message manager.
          childTransport.close();
          childTransport = null;
          connection.cancelForwarding(prefix);

          // ... and notify the child process to clean the target-scoped actors.
          try {
            mm.sendAsyncMessage("debug:content-process-destroy");
          } catch (e) {
            // Nothing to do
          }
        }

        if (onDestroy) {
          onDestroy(mm);
        }
      }

      const onMessageManagerClose = DevToolsUtils.makeInfallible(
        (subject, topic, data) => {
          if (subject == mm) {
            onClose();
            connection.send({ from: actor.actor, type: "tabDetached" });
          }
        }
      );
      Services.obs.addObserver(onMessageManagerClose, "message-manager-close");

      EventEmitter.on(connection, "closed", onClose);
    });
  },
};

exports.ContentProcessConnector = ContentProcessConnector;
