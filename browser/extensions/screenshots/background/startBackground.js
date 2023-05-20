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
(function () {
  let link = document.createElement("link");
  link.setAttribute("rel", "localization");
  link.setAttribute("href", "browser/screenshots.ftl");
  document.head.appendChild(link);

  link = document.createElement("link");
  link.setAttribute("rel", "localization");
  link.setAttribute("href", "toolkit/branding/brandings.ftl");
  document.head.appendChild(link);
})();

this.getStrings = async function (ids) {
  if (document.readyState != "complete") {
    await new Promise(resolve =>
      window.addEventListener("load", resolve, { once: true })
    );
  }
  await document.l10n.ready;
  return document.l10n.formatValues(ids);
};

let zoomFactor = 1;
this.getZoomFactor = function () {
  return zoomFactor;
};

this.startBackground = (function () {
  const exports = { startTime };

  const backgroundScripts = [
    "log.js",
    "catcher.js",
    "blobConverters.js",
    "background/selectorLoader.js",
    "background/communication.js",
    "background/senderror.js",
    "build/shot.js",
    "build/thumbnailGenerator.js",
    "background/analytics.js",
    "background/deviceInfo.js",
    "background/takeshot.js",
    "background/main.js",
  ];

  browser.experiments.screenshots.onScreenshotCommand.addListener(
    async type => {
      try {
        let [[tab]] = await Promise.all([
          browser.tabs.query({ currentWindow: true, active: true }),
          loadIfNecessary(),
        ]);
        zoomFactor = await browser.tabs.getZoom(tab.id);
        if (type === "contextMenu") {
          main.onClickedContextMenu(tab);
        } else if (type === "toolbar" || type === "quickaction") {
          main.onClicked(tab);
        } else if (type === "shortcut") {
          main.onShortcut(tab);
        }
      } catch (error) {
        console.error("Error loading Screenshots:", error);
      }
    }
  );

  browser.runtime.onMessage.addListener((req, sender, sendResponse) => {
    loadIfNecessary()
      .then(() => {
        return communication.onMessage(req, sender, sendResponse);
      })
      .catch(error => {
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
    backgroundScripts.forEach(script => {
      loadedPromise = loadedPromise.then(() => {
        return new Promise((resolve, reject) => {
          const tag = document.createElement("script");
          tag.src = browser.runtime.getURL(script);
          tag.onload = () => {
            resolve();
          };
          tag.onerror = error => {
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
