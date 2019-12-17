/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, XPCOMUtils */

XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

function getInfoFrameScript(messageName) {
  /* eslint-env mozilla/frame-script */

  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );
  const PREVIEW_MAX_ITEMS = 10;
  const LOG_LEVEL_MAP = {
    0: "debug",
    1: "info",
    2: "warn",
    3: "error",
  };

  function getInnerWindowId(window) {
    return window.windowUtils.currentInnerWindowID;
  }

  function getInnerWindowIDsForAllFrames(window) {
    const innerWindowID = getInnerWindowId(window);
    let ids = [innerWindowID];

    if (window.frames) {
      for (let i = 0; i < window.frames.length; i++) {
        ids = ids.concat(getInnerWindowIDsForAllFrames(window.frames[i]));
      }
    }

    return ids;
  }

  function getLoggedMessages(window, includePrivate = false) {
    const ids = getInnerWindowIDsForAllFrames(window);
    return getConsoleMessages(ids)
      .concat(getScriptErrors(ids, includePrivate))
      .sort((a, b) => a.timeStamp - b.timeStamp)
      .map(m => m.message);
  }

  function getPreview(value) {
    switch (typeof value) {
      case "function":
        return "function ()";

      case "object":
        if (value === null) {
          return null;
        }

        if (Array.isArray(value)) {
          return `(${value.length})[...]`;
        }

        return "{...}";

      case "undefined":
        return "undefined";

      default:
        return value;
    }
  }

  function getArrayPreview(arr) {
    const preview = [];
    for (const value of arr) {
      preview.push(getPreview(value));

      if (preview.length === PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return preview;
  }

  function getObjectPreview(obj) {
    const preview = {};
    for (const key in obj) {
      if (obj.hasOwnProperty(key)) {
        preview[key] = getPreview(obj[key]);
      }

      if (Object.keys(preview).length === PREVIEW_MAX_ITEMS) {
        break;
      }
    }

    return preview;
  }

  function getArgs(value) {
    if (typeof value === "object" && value !== null) {
      if (Array.isArray(value)) {
        return getArrayPreview(value);
      }

      return getObjectPreview(value);
    }

    return getPreview(value);
  }

  function getConsoleMessages(windowIds) {
    const ConsoleAPIStorage = Cc[
      "@mozilla.org/consoleAPI-storage;1"
    ].getService(Ci.nsIConsoleAPIStorage);
    let messages = [];
    for (const id of windowIds) {
      messages = messages.concat(ConsoleAPIStorage.getEvents(id) || []);
    }
    return messages.map(evt => {
      const { columnNumber, filename, level, lineNumber, timeStamp } = evt;
      const args = evt.arguments.map(getArgs);

      const message = {
        level,
        log: args,
        uri: filename,
        pos: `${lineNumber}:${columnNumber}`,
      };

      return { timeStamp, message };
    });
  }

  function getScriptErrors(windowIds, includePrivate = false) {
    const messages = Services.console.getMessageArray() || [];
    return messages
      .filter(message => {
        if (message instanceof Ci.nsIScriptError) {
          if (!includePrivate && message.isFromPrivateWindow) {
            return false;
          }

          if (windowIds && !windowIds.includes(message.innerWindowID)) {
            return false;
          }

          return true;
        }

        // If this is not an nsIScriptError and we need to do window-based
        // filtering we skip this message.
        return false;
      })
      .map(error => {
        const {
          timeStamp,
          errorMessage,
          sourceName,
          lineNumber,
          columnNumber,
          logLevel,
        } = error;
        const message = {
          level: LOG_LEVEL_MAP[logLevel],
          log: [errorMessage],
          uri: sourceName,
          pos: `${lineNumber}:${columnNumber}`,
        };

        return { timeStamp, message };
      });
  }

  docShell.getHasTrackingContentBlocked().then(hasTrackingContentBlocked => {
    sendAsyncMessage(messageName, {
      hasMixedActiveContentBlocked: docShell.hasMixedActiveContentBlocked,
      hasMixedDisplayContentBlocked: docShell.hasMixedDisplayContentBlocked,
      hasTrackingContentBlocked,
      log: getLoggedMessages(content),
    });
  });
}

this.tabExtras = class extends ExtensionAPI {
  getAPI(context) {
    const { tabManager } = context.extension;

    return {
      tabExtras: {
        async getWebcompatInfo(tabId) {
          return new Promise(resolve => {
            const messageName = "WebExtension:GetWebcompatInfo";
            const code = `${getInfoFrameScript.toString()};getInfoFrameScript("${messageName}")`;
            const mm = tabManager.get(tabId).browser.messageManager;
            mm.loadFrameScript(`data:,${encodeURI(code)}`, false);
            mm.addMessageListener(messageName, function receiveFn(message) {
              mm.removeMessageListener(messageName, receiveFn);
              resolve(message.json);
            });
          });
        },
      },
    };
  }
};
