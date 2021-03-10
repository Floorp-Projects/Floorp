/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals browser, main, communication, manifest */
/* This file handles:
     clicks on the WebExtension page action
     browser.contextMenus.onClicked
     browser.runtime.onMessage
   and loads the rest of the background page in response to those events, forwarding
   the events to main.onClicked, main.onClickedContextMenu, or communication.onMessage
*/
const startTime = Date.now();

// Set up to be able to use fluent:
(function() {
  let link = document.createElement("link");
  link.setAttribute("rel", "localization");
  link.setAttribute("href", "browser/screenshots.ftl");
  document.head.appendChild(link);

  link = document.createElement("link");
  link.setAttribute("rel", "localization");
  link.setAttribute("href", "browser/branding/brandings.ftl");
  document.head.appendChild(link);
})();

this.getStrings = async function(ids) {
  if (document.readyState != "complete") {
    await new Promise(resolve => window.addEventListener("load", resolve, {once: true}));
  }
  await document.l10n.ready;
  return document.l10n.formatValues(ids);
}

this.startBackground = (function() {
  const exports = {startTime};

  const backgroundScripts = [
    "log.js",
    "makeUuid.js",
    "catcher.js",
    "blobConverters.js",
    "background/selectorLoader.js",
    "background/communication.js",
    "background/auth.js",
    "background/senderror.js",
    "build/raven.js",
    "build/shot.js",
    "build/thumbnailGenerator.js",
    "background/analytics.js",
    "background/deviceInfo.js",
    "background/takeshot.js",
    "background/main.js",
  ];

  browser.pageAction.onClicked.addListener(tab => {
    loadIfNecessary().then(() => {
      main.onClicked(tab);
    }).catch(error => {
      console.error("Error loading Screenshots:", error);
    });
  });

  browser.experiments.screenshots.onScreenshotCommand.addListener(async () => {
    try {
      let [[tab]] = await Promise.all([
        browser.tabs.query({currentWindow: true, active: true}),
        loadIfNecessary(),
      ]);
      main.onClickedContextMenu(tab);
    } catch (error) {
      console.error("Error loading Screenshots:", error);
    }
  });

  browser.commands.onCommand.addListener((cmd) => {
    if (cmd !== "take-screenshot") {
      return;
    }
    loadIfNecessary().then(() => {
      browser.tabs.query({currentWindow: true, active: true}).then((tabs) => {
        const activeTab = tabs[0];
        main.onCommand(activeTab);
      }).catch((error) => {
        throw error;
      });
    }).catch((error) => {
      console.error("Error toggling Screenshots via keyboard shortcut: ", error);
    });
  });

  browser.runtime.onMessage.addListener((req, sender, sendResponse) => {
    loadIfNecessary().then(() => {
      return communication.onMessage(req, sender, sendResponse);
    }).catch((error) => {
      console.error("Error loading Screenshots:", error);
    });
    return true;
  });

  let loadedPromise;

  function loadIfNecessary() {
    if (loadedPromise) {
      return loadedPromise;
    }
    loadedPromise = Promise.resolve();
    backgroundScripts.forEach((script) => {
      loadedPromise = loadedPromise.then(() => {
        return new Promise((resolve, reject) => {
          const tag = document.createElement("script");
          tag.src = browser.extension.getURL(script);
          tag.onload = () => {
            resolve();
          };
          tag.onerror = (error) => {
            const exc = new Error(`Error loading script: ${error.message}`);
            exc.scriptName = script;
            reject(exc);
          };
          document.head.appendChild(tag);
        });
      });
    });
    return loadedPromise;
  }

  return exports;
})();
