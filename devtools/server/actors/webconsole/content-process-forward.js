/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm", {});

ChromeUtils.defineModuleGetter(this, "E10SUtils",
                               "resource://gre/modules/E10SUtils.jsm");

/*
 * The message manager has an upper limit on message sizes that it can
 * reliably forward to the parent so we limit the size of console log event
 * messages that we forward here. The web console is local and receives the
 * full console message, but addons subscribed to console event messages
 * in the parent receive the truncated version. Due to fragmentation,
 * messages as small as 1MB have resulted in IPC allocation failures on
 * 32-bit platforms. To limit IPC allocation sizes, console.log messages
 * with arguments with total size > MSG_MGR_CONSOLE_MAX_SIZE (bytes) have
 * their arguments completely truncated. MSG_MGR_CONSOLE_VAR_SIZE is an
 * approximation of how much space (in bytes) a JS non-string variable will
 * require in the manager's implementation. For strings, we use 2 bytes per
 * char. The console message URI and function name are limited to
 * MSG_MGR_CONSOLE_INFO_MAX characters. We don't attempt to calculate
 * the exact amount of space the message manager implementation will require
 * for a given message so this is imperfect.
 */
const MSG_MGR_CONSOLE_MAX_SIZE = 1024 * 1024; // 1MB
const MSG_MGR_CONSOLE_VAR_SIZE = 8;
const MSG_MGR_CONSOLE_INFO_MAX = 1024;

function ContentProcessForward() {
  Services.obs.addObserver(this, "console-api-log-event");
  Services.obs.addObserver(this, "xpcom-shutdown");
  Services.cpmm.addMessageListener("DevTools:StopForwardingContentProcessMessage", this);
}
ContentProcessForward.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),

  receiveMessage(message) {
    if (message.name == "DevTools:StopForwardingContentProcessMessage") {
      this.uninit();
    }
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "console-api-log-event": {
        let consoleMsg = subject.wrappedJSObject;

        let msgData = {
          ...consoleMsg,
          arguments: [],
          filename: consoleMsg.filename.substring(0, MSG_MGR_CONSOLE_INFO_MAX),
          functionName: consoleMsg.functionName &&
            consoleMsg.functionName.substring(0, MSG_MGR_CONSOLE_INFO_MAX),
          // Prevents cyclic object error when using msgData in sendAsyncMessage
          wrappedJSObject: null,
        };

        // We can't send objects over the message manager, so we sanitize
        // them out, replacing those arguments with "<unavailable>".
        let unavailString = "<unavailable>";
        let unavailStringLength = unavailString.length * 2; // 2-bytes per char

        // When the sum of argument sizes reaches MSG_MGR_CONSOLE_MAX_SIZE,
        // replace all arguments with "<truncated>".
        let totalArgLength = 0;

        // Walk through the arguments, checking the type and size.
        for (let arg of consoleMsg.arguments) {
          if ((typeof arg == "object" || typeof arg == "function") &&
              arg !== null) {
            if (Services.appinfo.remoteType === E10SUtils.EXTENSION_REMOTE_TYPE) {
              // For OOP extensions: we want the developer to be able to see the
              // logs in the Browser Console. When the Addon Toolbox will be more
              // prominent we can revisit.
              try {
                // If the argument is clonable, then send it as-is. If
                // cloning fails, fall back to the unavailable string.
                arg = Cu.cloneInto(arg, {});
              } catch (e) {
                arg = unavailString;
              }
            } else {
              arg = unavailString;
            }
            totalArgLength += unavailStringLength;
          } else if (typeof arg == "string") {
            totalArgLength += arg.length * 2; // 2-bytes per char
          } else {
            totalArgLength += MSG_MGR_CONSOLE_VAR_SIZE;
          }

          if (totalArgLength <= MSG_MGR_CONSOLE_MAX_SIZE) {
            msgData.arguments.push(arg);
          } else {
            // arguments take up too much space
            msgData.arguments = ["<truncated>"];
            break;
          }
        }

        Services.cpmm.sendAsyncMessage("Console:Log", msgData);
        break;
      }

      case "xpcom-shutdown":
        this.uninit();
        break;
    }
  },

  uninit() {
    Services.obs.removeObserver(this, "console-api-log-event");
    Services.obs.removeObserver(this, "xpcom-shutdown");
    Services.cpmm.removeMessageListener("DevTools:StopForwardingContentProcessMessage",
                                        this);
  }
};

// loadProcessScript loads in all processes, including the parent,
// in which we don't need any forwarding
if (Services.appinfo.processType == Services.appinfo.PROCESS_TYPE_CONTENT) {
  new ContentProcessForward();
}

