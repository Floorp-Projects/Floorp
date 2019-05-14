/* globals selectorLoader, analytics, communication, catcher, log, makeUuid, auth, senderror, startBackground, blobConverters buildSettings */

"use strict";

this.main = (function() {
  const exports = {};

  const pasteSymbol = (window.navigator.platform.match(/Mac/i)) ? "\u2318" : "Ctrl";
  const { sendEvent, incrementCount } = analytics;

  const manifest = browser.runtime.getManifest();
  let backend;

  exports.hasAnyShots = function() {
    return false;
  };

  let hasSeenOnboarding = browser.storage.local.get(["hasSeenOnboarding"]).then((result) => {
    const onboarded = !!result.hasSeenOnboarding;
    hasSeenOnboarding = Promise.resolve(onboarded);
    return hasSeenOnboarding;
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

  for (const permission of manifest.permissions) {
    if (/^https?:\/\//.test(permission)) {
      exports.setBackend(permission);
      break;
    }
  }

  function setIconActive(active, tabId) {
    const path = active ? "icons/icon-highlight-32-v2.svg" : "icons/icon-v2.svg";
    browser.pageAction.setIcon({tabId, path});
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
        sendEvent("start-shot", "site-request", {incognito: tab.incognito});
        setIconActive(true, tab.id);
        selectorLoader.toggle(tab.id, Promise.resolve(false));
      }
    });
  }

  function shouldOpenMyShots(url) {
    return /^about:(?:newtab|blank|home)/i.test(url) || /^resource:\/\/activity-streams\//i.test(url);
  }

  // This is called by startBackground.js, where is registered as a click
  // handler for the webextension page action.
  exports.onClicked = catcher.watchFunction((tab) => {
    _startShotFlow(tab, "toolbar-button");
  });

  function forceOnboarding() {
    return browser.tabs.create({url: getOnboardingUrl()});
  }

  exports.onClickedContextMenu = catcher.watchFunction((info, tab) => {
    _startShotFlow(tab, "context-menu");
  });

  exports.onCommand = catcher.watchFunction((tab) => {
    _startShotFlow(tab, "keyboard-shortcut");
  });

  const _openMyShots = (tab, inputType) => {
    catcher.watchPromise(analytics.refreshTelemetryPref().then(() => {
      sendEvent("goto-myshots", inputType, {incognito: tab.incognito});
    }));
    catcher.watchPromise(
      auth.maybeLogin()
      .then(() => browser.tabs.update({url: backend + "/shots"})));
  };

  const _startShotFlow = (tab, inputType) => {
    catcher.watchPromise(hasSeenOnboarding.then(onboarded => {
      if (!tab) {
        // Not in a page/tab context, ignore
        return;
      }
      if (!urlEnabled(tab.url)) {
        if (!onboarded) {
          // Updated generic "selection-button" event data to inputType
          sendEvent("goto-onboarding", inputType, {incognito: tab.incognito});
          forceOnboarding();
          return;
        }
        senderror.showError({
          popupMessage: "UNSHOOTABLE_PAGE",
        });
        return;
      } else if (shouldOpenMyShots(tab.url)) {
        _openMyShots(tab, inputType);
        return;
      }
      // No need to catch() here because of watchPromise().
      // eslint-disable-next-line promise/catch-or-return
      toggleSelector(tab)
        .then(active => {
          let event = "start-shot";
          if (inputType !== "context-menu") {
            event = active ? "start-shot" : "cancel-shot";
          }
          sendEvent(event, inputType, {incognito: tab.incognito});
        }).catch((error) => {
        throw error;
      });
    }));
  };

  function urlEnabled(url) {
    if (shouldOpenMyShots(url)) {
      return true;
    }
    // Allow screenshots on urls related to web pages in reader mode.
    if (url && url.startsWith("about:reader?url=")) {
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
    const path = url.substr(backend.length).replace(/^\/*/, "").replace(/[?#].*/, "");
    if (path === "shots") {
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
    const badDomains = ["testpilot.firefox.com"];
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
      auth.maybeLogin()
      .then(() => browser.tabs.create({url: backend + "/shots"})));
  });

  communication.register("openShot", (sender, {url, copied}) => {
    if (copied) {
      const id = makeUuid();
      return browser.notifications.create(id, {
        type: "basic",
        iconUrl: "../icons/copied-notification.svg",
        title: browser.i18n.getMessage("notificationLinkCopiedTitle"),
        message: browser.i18n.getMessage("notificationLinkCopiedDetails", pasteSymbol),
      });
    }
    return null;
  });

  // This is used for truncated full page downloads and copy to clipboards.
  // Those longer operations need to display an animated spinner/loader, so
  // it's preferable to perform toDataURL() in the background.
  communication.register("canvasToDataURL", (sender, imageData) => {
    const canvas = document.createElement("canvas");
    canvas.width = imageData.width;
    canvas.height = imageData.height;
    canvas.getContext("2d").putImageData(imageData, 0, 0);
    let dataUrl = canvas.toDataURL();
    if (buildSettings.pngToJpegCutoff && dataUrl.length > buildSettings.pngToJpegCutoff) {
      const jpegDataUrl = canvas.toDataURL("image/jpeg");
      if (jpegDataUrl.length < dataUrl.length) {
        // Only use the JPEG if it is actually smaller
        dataUrl = jpegDataUrl;
      }
    }
    return dataUrl;
  });

  communication.register("copyShotToClipboard", (sender, blob) => {
    return blobConverters.blobToArray(blob).then(buffer => {
      return browser.clipboard.setImageData(
        buffer, blob.type.split("/", 2)[1]).then(() => {
          catcher.watchPromise(incrementCount("copy"));
          return browser.notifications.create({
            type: "basic",
            iconUrl: "../icons/copied-notification.svg",
            title: browser.i18n.getMessage("notificationImageCopiedTitle"),
            message: browser.i18n.getMessage("notificationImageCopiedDetails", pasteSymbol),
          });
        });
    });
  });

  communication.register("downloadShot", (sender, info) => {
    // 'data:' urls don't work directly, let's use a Blob
    // see http://stackoverflow.com/questions/40269862/save-data-uri-as-file-using-downloads-download-api
    const blob = blobConverters.dataUrlToBlob(info.url);
    const url = URL.createObjectURL(blob);
    let downloadId;
    const onChangedCallback = catcher.watchFunction(function(change) {
      if (!downloadId || downloadId !== change.id) {
        return;
      }
      if (change.state && change.state.current !== "in_progress") {
        URL.revokeObjectURL(url);
        browser.downloads.onChanged.removeListener(onChangedCallback);
      }
    });
    browser.downloads.onChanged.addListener(onChangedCallback);
    catcher.watchPromise(incrementCount("download"));
    return browser.windows.getLastFocused().then(windowInfo => {
      return browser.downloads.download({
        url,
        incognito: windowInfo.incognito,
        filename: info.filename,
      }).catch((error) => {
        // We are not logging error message when user cancels download
        if (error && error.message && !error.message.includes("canceled")) {
          log.error(error.message);
        }
      }).then((id) => {
        downloadId = id;
      });
    });
  });

  communication.register("closeSelector", (sender) => {
    setIconActive(false, sender.tab.id);
  });

  communication.register("hasSeenOnboarding", () => {
    hasSeenOnboarding = Promise.resolve(true);
    catcher.watchPromise(browser.storage.local.set({hasSeenOnboarding: true}));
  });

  communication.register("abortStartShot", () => {
    // Note, we only show the error but don't report it, as we know that we can't
    // take shots of these pages:
    senderror.showError({
      popupMessage: "UNSHOOTABLE_PAGE",
    });
  });

  // A Screenshots page wants us to start/force onboarding
  communication.register("requestOnboarding", (sender) => {
    return startSelectionWithOnboarding(sender.tab);
  });

  communication.register("getPlatformOs", () => {
    return catcher.watchPromise(browser.runtime.getPlatformInfo().then(platformInfo => {
      return platformInfo.os;
    }));
  });

  // This allows the web site show notifications through sitehelper.js
  communication.register("showNotification", (sender, notification) => {
    return browser.notifications.create(notification);
  });

  return exports;
})();
