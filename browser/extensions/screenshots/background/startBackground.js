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
  // Wait until this many milliseconds to check the server for shots (for the purpose of migration warning):
  const CHECK_SERVER_TIME = 5000; // 5 seconds
  // If we want to pop open the tab showing the server status, wait this many milliseconds to open it:
  const OPEN_SERVER_TAB_TIME = 5000; // 5 seconds
  let hasSeenServerStatus = false;
  let _resolveServerStatus;
  exports.serverStatus = new Promise((resolve, reject) => {
    _resolveServerStatus = {resolve, reject};
  });
  let backend;

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

  async function checkExpiration() {
    const manifest = await browser.runtime.getManifest();
    for (const permission of manifest.permissions) {
      if (/^https?:\/\//.test(permission)) {
        backend = permission.replace(/\/*$/, "");
        break;
      }
    }
    const result = await browser.storage.local.get(["registrationInfo", "hasSeenServerStatus", "hasShotsResponse"]);
    hasSeenServerStatus = result.hasSeenServerStatus;
    const { registrationInfo } = result;
    if (!backend || !registrationInfo || !registrationInfo.registered) {
      // The add-on hasn't been used, or at least no upload has occurred
      _resolveServerStatus.resolve({hasIndefinite: false, hasAny: false});
      return;
    }
    if (result.hasShotsResponse) {
      // We've already retrieved information from the server
      _resolveServerStatus.resolve(result.hasShotsResponse);
      return;
    }
    const loginUrl = `${backend}/api/login`;
    const hasShotsUrl = `${backend}/api/has-shots`;
    try {
      let resp = await fetch(loginUrl, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          deviceId: registrationInfo.deviceId,
          secret: registrationInfo.secret,
        }),
      });
      if (!resp.ok) {
        throw new Error(`Bad login response: ${resp.status}`);
      }
      const { authHeader } = await resp.json();
      resp = await fetch(hasShotsUrl, {
        method: "GET",
        credentials: "include",
        headers: Object.assign({}, authHeader, {
          "Content-Type": "application/json",
        }),
      });
      if (!resp.ok) {
        throw new Error(`Bad response from server: ${resp.status}`);
      }
      const body = await resp.json();
      browser.storage.local.set({hasShotsResponse: body});
      _resolveServerStatus.resolve(body);
    } catch (e) {
      _resolveServerStatus.reject(e);
    }
  }

  exports.serverStatus.then((status) => {
    if (status.hasAny) {
      browser.experiments.screenshots.initLibraryButton();
    }
    if (status.hasIndefinite && !hasSeenServerStatus) {
      setTimeout(async () => {
        await browser.tabs.create({
          url: `${backend}/hosting-shutdown`,
        });
        hasSeenServerStatus = true;
        await browser.storage.local.set({hasSeenServerStatus});
      }, OPEN_SERVER_TAB_TIME);
    }
  }).catch((e) => {
    console.error("Error finding Screenshots server status:", String(e), e.stack);
  });

  setTimeout(() => {
    window.requestIdleCallback(() => {
      checkExpiration();
    });
  }, CHECK_SERVER_TIME);

  return exports;
})();
