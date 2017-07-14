/* globals selectorLoader, analytics, communication, catcher, log, makeUuid, auth, senderror */

"use strict";

this.main = (function() {
  let exports = {};

  const pasteSymbol = (window.navigator.platform.match(/Mac/i)) ? "\u2318" : "Ctrl";
  const { sendEvent } = analytics;

  let manifest = browser.runtime.getManifest();
  let backend;

  let hasSeenOnboarding;

  browser.storage.local.get(["hasSeenOnboarding"]).then((result) => {
    hasSeenOnboarding = !!result.hasSeenOnboarding;
    if (!hasSeenOnboarding) {
      setIconActive(false, null);
      // Note that the branded name 'Firefox Screenshots' is not localized:
      browser.browserAction.setTitle({
        title: "Firefox Screenshots"
      });
    }
  }).catch((error) => {
    log.error("Error getting hasSeenOnboarding:", error);
  });

  exports.setBackend = function(newBackend) {
    backend = newBackend;
    backend = backend.replace(/\/*$/, "");
  };

  exports.getBackend = function() {
    return backend;
  };

  communication.register("getBackend", () => {
    return backend;
  });

  function getOnboardingUrl() {
    return backend + "/#hello";
  }

  for (let permission of manifest.permissions) {
    if (/^https?:\/\//.test(permission)) {
      exports.setBackend(permission);
      break;
    }
  }

  function setIconActive(active, tabId) {
    let path = active ? "icons/icon-highlight-32-v2.svg" : "icons/icon-32-v2.svg";
    if ((!hasSeenOnboarding) && !active) {
      path = "icons/icon-starred-32-v2.svg";
    }
    browser.browserAction.setIcon({path, tabId}).catch((error) => {
      // FIXME: use errorCode
      if (error.message && /Invalid tab ID/.test(error.message)) {
        // This is a normal exception that we can ignore
      } else {
        catcher.unhandled(error);
      }
    });
  }

  function toggleSelector(tab) {
    return analytics.refreshTelemetryPref()
      .then(() => selectorLoader.toggle(tab.id, hasSeenOnboarding))
      .then(active => {
        setIconActive(active, tab.id);
        return active;
      })
      .catch((error) => {
        if (error.message && /Missing host permission for the tab/.test(error.message)) {
          error.noReport = true;
        }
        error.popupMessage = "UNSHOOTABLE_PAGE";
        throw error;
      });
  }

  function startSelectionWithOnboarding(tab) {
    return analytics.refreshTelemetryPref().then(() => {
      return selectorLoader.testIfLoaded(tab.id);
    }).then((isLoaded) => {
      if (!isLoaded) {
        sendEvent("start-shot", "site-request");
        setIconActive(true, tab.id);
        selectorLoader.toggle(tab.id, false);
      }
    });
  }

  function shouldOpenMyShots(url) {
    return /^about:(?:newtab|blank)/i.test(url) || /^resource:\/\/activity-streams\//i.test(url);
  }

  // This is called by startBackground.js, directly in response to browser.browserAction.onClicked
  exports.onClicked = catcher.watchFunction((tab) => {
    if (tab.incognito) {
      senderror.showError({
        popupMessage: "PRIVATE_WINDOW"
      });
      return;
    }
    if (shouldOpenMyShots(tab.url)) {
      if (!hasSeenOnboarding) {
        catcher.watchPromise(analytics.refreshTelemetryPref().then(() => {
          sendEvent("goto-onboarding", "selection-button");
          return forceOnboarding();
        }));
        return;
      }
      catcher.watchPromise(analytics.refreshTelemetryPref().then(() => {
        sendEvent("goto-myshots", "about-newtab");
      }));
      catcher.watchPromise(
        auth.authHeaders()
        .then(() => browser.tabs.update({url: backend + "/shots"})));
    } else {
      catcher.watchPromise(
        toggleSelector(tab)
          .then(active => {
            const event = active ? "start-shot" : "cancel-shot";
            sendEvent(event, "toolbar-button");
          }, (error) => {
            if ((!hasSeenOnboarding) && error.popupMessage == "UNSHOOTABLE_PAGE") {
              sendEvent("goto-onboarding", "selection-button");
              return forceOnboarding();
            }
            throw error;
          }));
    }
  });

  function forceOnboarding() {
    return browser.tabs.create({url: getOnboardingUrl()});
  }

  exports.onClickedContextMenu = catcher.watchFunction((info, tab) => {
    if (!tab) {
      // Not in a page/tab context, ignore
      return;
    }
    if (tab.incognito) {
      senderror.showError({
        popupMessage: "PRIVATE_WINDOW"
      });
      return;
    }
    if (!urlEnabled(tab.url)) {
      senderror.showError({
        popupMessage: "UNSHOOTABLE_PAGE"
      });
      return;
    }
    catcher.watchPromise(
      toggleSelector(tab)
        .then(() => sendEvent("start-shot", "context-menu")));
  });

  function urlEnabled(url) {
    if (shouldOpenMyShots(url)) {
      return true;
    }
    if (isShotOrMyShotPage(url) || /^(?:about|data|moz-extension):/i.test(url) || isBlacklistedUrl(url)) {
      return false;
    }
    return true;
  }

  function isShotOrMyShotPage(url) {
    // It's okay to take a shot of any pages except shot pages and My Shots
    if (!url.startsWith(backend)) {
      return false;
    }
    let path = url.substr(backend.length).replace(/^\/*/, "").replace(/[?#].*/, "");
    if (path == "shots") {
      return true;
    }
    if (/^[^/]{1,4000}\/[^/]{1,4000}$/.test(path)) {
      // Blocks {:id}/{:domain}, but not /, /privacy, etc
      return true;
    }
    return false;
  }

  function isBlacklistedUrl(url) {
    // These specific domains are not allowed for general WebExtension permission reasons
    // Discussion: https://bugzilla.mozilla.org/show_bug.cgi?id=1310082
    // List of domains copied from: https://dxr.mozilla.org/mozilla-central/source/browser/app/permissions#18-19
    // Note we disable it here to be informative, the security check is done in WebExtension code
    const badDomains = ["addons.mozilla.org", "testpilot.firefox.com"];
    let domain = url.replace(/^https?:\/\//i, "");
    domain = domain.replace(/\/.*/, "").replace(/:.*/, "");
    domain = domain.toLowerCase();
    return badDomains.includes(domain);
  }

  communication.register("sendEvent", (sender, ...args) => {
    catcher.watchPromise(sendEvent(...args));
    // We don't wait for it to complete:
    return null;
  });

  communication.register("openMyShots", (sender) => {
    return catcher.watchPromise(
      auth.authHeaders()
      .then(() => browser.tabs.create({url: backend + "/shots"})));
  });

  communication.register("openShot", (sender, {url, copied}) => {
    if (copied) {
      const id = makeUuid();
      return browser.notifications.create(id, {
        type: "basic",
        iconUrl: "../icons/copy.png",
        title: browser.i18n.getMessage("notificationLinkCopiedTitle"),
        message: browser.i18n.getMessage("notificationLinkCopiedDetails", pasteSymbol)
      });
    }
  });

  communication.register("downloadShot", (sender, info) => {
    // 'data:' urls don't work directly, let's use a Blob
    // see http://stackoverflow.com/questions/40269862/save-data-uri-as-file-using-downloads-download-api
    const binary = atob(info.url.split(',')[1]); // just the base64 data
    const data = Uint8Array.from(binary, char => char.charCodeAt(0))
    const blob = new Blob([data], {type: "image/png"})
    let url = URL.createObjectURL(blob);
    let downloadId;
    let onChangedCallback = catcher.watchFunction(function(change) {
      if (!downloadId || downloadId != change.id) {
        return;
      }
      if (change.state && change.state.current != "in_progress") {
        URL.revokeObjectURL(url);
        browser.downloads.onChanged.removeListener(onChangedCallback);
      }
    });
    browser.downloads.onChanged.addListener(onChangedCallback)
    return browser.downloads.download({
      url,
      filename: info.filename
    }).then((id) => {
      downloadId = id;
    });
  });

  communication.register("closeSelector", (sender) => {
    setIconActive(false, sender.tab.id);
  });

  catcher.watchPromise(communication.sendToBootstrap("getOldDeviceInfo").then((deviceInfo) => {
    if (deviceInfo === communication.NO_BOOTSTRAP || !deviceInfo) {
      return;
    }
    deviceInfo = JSON.parse(deviceInfo);
    if (deviceInfo && typeof deviceInfo == "object") {
      return auth.setDeviceInfoFromOldAddon(deviceInfo).then((updated) => {
        if (updated === communication.NO_BOOTSTRAP) {
          throw new Error("bootstrap.js disappeared unexpectedly");
        }
        if (updated) {
          return communication.sendToBootstrap("removeOldAddon");
        }
      });
    }
  }));

  communication.register("hasSeenOnboarding", () => {
    hasSeenOnboarding = true;
    catcher.watchPromise(browser.storage.local.set({hasSeenOnboarding}));
    setIconActive(false, null);
    browser.browserAction.setTitle({
      title: browser.i18n.getMessage("contextMenuLabel")
    });
  });

  communication.register("abortFrameset", () => {
    sendEvent("abort-start-shot", "frame-page");
    // Note, we only show the error but don't report it, as we know that we can't
    // take shots of these pages:
    senderror.showError({
      popupMessage: "UNSHOOTABLE_PAGE"
    });
  });

  communication.register("abortNoDocumentBody", (sender, tagName) => {
    tagName = String(tagName || "").replace(/[^a-z0-9]/ig, "");
    sendEvent("abort-start-shot", `document-is-${tagName}`);
    // Note, we only show the error but don't report it, as we know that we can't
    // take shots of these pages:
    senderror.showError({
      popupMessage: "UNSHOOTABLE_PAGE"
    });
  });

  // Note: this signal is only needed until bug 1357589 is fixed.
  communication.register("openTermsPage", () => {
    return catcher.watchPromise(browser.tabs.create({url: "https://www.mozilla.org/about/legal/terms/services/"}));
  });

  // Note: this signal is also only needed until bug 1357589 is fixed.
  communication.register("openPrivacyPage", () => {
    return catcher.watchPromise(browser.tabs.create({url: "https://www.mozilla.org/privacy/firefox-cloud/"}));
  });

  // A Screenshots page wants us to start/force onboarding
  communication.register("requestOnboarding", (sender) => {
    return startSelectionWithOnboarding(sender.tab);
  });

  return exports;
})();
