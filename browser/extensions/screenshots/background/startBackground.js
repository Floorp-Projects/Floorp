/* globals browser, main, communication, manifest */
/* This file handles:
     clicks on the WebExtension page action
     browser.contextMenus.onClicked
     browser.runtime.onMessage
   and loads the rest of the background page in response to those events, forwarding
   the events to main.onClicked, main.onClickedContextMenu, or communication.onMessage
*/
const startTime = Date.now();

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

  browser.contextMenus.create({
    id: "create-screenshot",
    title: browser.i18n.getMessage("contextMenuLabel"),
    contexts: ["page", "selection"],
    documentUrlPatterns: ["<all_urls>", "about:reader*"],
  });

  browser.contextMenus.onClicked.addListener((info, tab) => {
    loadIfNecessary().then(() => {
      main.onClickedContextMenu(info, tab);
    }).catch((error) => {
      console.error("Error loading Screenshots:", error);
    });
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
