/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals browser, getStrings, selectorLoader, analytics, communication, catcher, log, makeUuid, auth, senderror, startBackground, blobConverters buildSettings, startSelectionWithOnboarding */

"use strict";

this.main = (function() {
  const exports = {};

  const { sendEvent, incrementCount } = analytics;

  const manifest = browser.runtime.getManifest();
  let backend;

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

  for (const permission of manifest.permissions) {
    if (/^https?:\/\//.test(permission)) {
      exports.setBackend(permission);
      break;
    }
  }

  function setIconActive(active) {
    browser.experiments.screenshots.setIcon(active);
  }

  function toggleSelector(tab) {
    return analytics
      .refreshTelemetryPref()
      .then(() => selectorLoader.toggle(tab.id))
      .then(active => {
        setIconActive(active);
        return active;
      })
      .catch(error => {
        if (
          error.message &&
          /Missing host permission for the tab/.test(error.message)
        ) {
          error.noReport = true;
        }
        error.popupMessage = "UNSHOOTABLE_PAGE";
        throw error;
      });
  }

  // This is called by startBackground.js, where is registered as a click
  // handler for the webextension page action.
  exports.onClicked = catcher.watchFunction(tab => {
    _startShotFlow(tab, "toolbar-button");
  });

  exports.onClickedContextMenu = catcher.watchFunction(tab => {
    _startShotFlow(tab, "context-menu");
  });

  exports.onCommand = catcher.watchFunction(tab => {
    _startShotFlow(tab, "keyboard-shortcut");
  });

  const _startShotFlow = (tab, inputType) => {
    if (!tab) {
      // Not in a page/tab context, ignore
      return;
    }
    if (!urlEnabled(tab.url)) {
      senderror.showError({
        popupMessage: "UNSHOOTABLE_PAGE",
      });
      return;
    }

    catcher.watchPromise(
      toggleSelector(tab)
        .then(active => {
          let event = "start-shot";
          if (inputType !== "context-menu") {
            event = active ? "start-shot" : "cancel-shot";
          }
          sendEvent(event, inputType, { incognito: tab.incognito });
        })
        .catch(error => {
          throw error;
        })
    );
  };

  function urlEnabled(url) {
    // Allow screenshots on urls related to web pages in reader mode.
    if (url && url.startsWith("about:reader?url=")) {
      return true;
    }
    if (
      isShotOrMyShotPage(url) ||
      /^(?:about|data|moz-extension):/i.test(url) ||
      isBlacklistedUrl(url)
    ) {
      return false;
    }
    return true;
  }

  function isShotOrMyShotPage(url) {
    // It's okay to take a shot of any pages except shot pages and My Shots
    if (!url.startsWith(backend)) {
      return false;
    }
    const path = url
      .substr(backend.length)
      .replace(/^\/*/, "")
      .replace(/[?#].*/, "");
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
    // List of domains copied from: https://searchfox.org/mozilla-central/source/browser/app/permissions#18-19
    // Note we disable it here to be informative, the security check is done in WebExtension code
    const badDomains = ["testpilot.firefox.com"];
    let domain = url.replace(/^https?:\/\//i, "");
    domain = domain.replace(/\/.*/, "").replace(/:.*/, "");
    domain = domain.toLowerCase();
    return badDomains.includes(domain);
  }

  communication.register("getStrings", (sender, ids) => {
    return getStrings(ids.map(id => ({ id })));
  });

  communication.register("sendEvent", (sender, ...args) => {
    catcher.watchPromise(sendEvent(...args));
    // We don't wait for it to complete:
    return null;
  });

  communication.register("openShot", async (sender, { url, copied }) => {
    if (copied) {
      const id = makeUuid();
      const [title, message] = await getStrings([
        { id: "screenshots-notification-link-copied-title" },
        { id: "screenshots-notification-link-copied-details" },
      ]);
      return browser.notifications.create(id, {
        type: "basic",
        iconUrl: "../icons/copied-notification.svg",
        title,
        message,
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
    if (
      buildSettings.pngToJpegCutoff &&
      dataUrl.length > buildSettings.pngToJpegCutoff
    ) {
      const jpegDataUrl = canvas.toDataURL("image/jpeg");
      if (jpegDataUrl.length < dataUrl.length) {
        // Only use the JPEG if it is actually smaller
        dataUrl = jpegDataUrl;
      }
    }
    return dataUrl;
  });

  communication.register("copyShotToClipboard", async (sender, blob) => {
    let buffer = await blobConverters.blobToArray(blob);
    await browser.clipboard.setImageData(buffer, blob.type.split("/", 2)[1]);

    const [title, message] = await getStrings([
      { id: "screenshots-notification-image-copied-title" },
      { id: "screenshots-notification-image-copied-details" },
    ]);

    catcher.watchPromise(incrementCount("copy"));
    return browser.notifications.create({
      type: "basic",
      iconUrl: "../icons/copied-notification.svg",
      title,
      message,
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
      return browser.downloads
        .download({
          url,
          incognito: windowInfo.incognito,
          filename: info.filename,
        })
        .catch(error => {
          // We are not logging error message when user cancels download
          if (error && error.message && !error.message.includes("canceled")) {
            log.error(error.message);
          }
        })
        .then(id => {
          downloadId = id;
        });
    });
  });

  communication.register("closeSelector", sender => {
    setIconActive(false);
  });

  communication.register("abortStartShot", () => {
    // Note, we only show the error but don't report it, as we know that we can't
    // take shots of these pages:
    senderror.showError({
      popupMessage: "UNSHOOTABLE_PAGE",
    });
  });

  // A Screenshots page wants us to start/force onboarding
  communication.register("requestOnboarding", sender => {
    return startSelectionWithOnboarding(sender.tab);
  });

  communication.register("getPlatformOs", () => {
    return catcher.watchPromise(
      browser.runtime.getPlatformInfo().then(platformInfo => {
        return platformInfo.os;
      })
    );
  });

  // This allows the web site show notifications through sitehelper.js
  communication.register("showNotification", (sender, notification) => {
    return browser.notifications.create(notification);
  });

  return exports;
})();
