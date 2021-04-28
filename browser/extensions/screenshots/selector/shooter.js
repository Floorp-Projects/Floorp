/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals global, browser, documentMetadata, util, uicontrol, ui, catcher */
/* globals domainFromUrl, randomString, shot, blobConverters */

"use strict";

this.shooter = (function() {
  // eslint-disable-line no-unused-vars
  const exports = {};
  const { AbstractShot } = shot;

  const RANDOM_STRING_LENGTH = 16;
  let backend;
  let shotObject;
  const callBackground = global.callBackground;

  function regexpEscape(str) {
    // http://stackoverflow.com/questions/3115150/how-to-escape-regular-expression-special-characters-using-javascript
    return str.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, "\\$&");
  }

  function sanitizeError(data) {
    const href = new RegExp(regexpEscape(window.location.href), "g");
    const origin = new RegExp(
      `${regexpEscape(window.location.origin)}[^ \t\n\r",>]*`,
      "g"
    );
    const json = JSON.stringify(data)
      .replace(href, "REDACTED_HREF")
      .replace(origin, "REDACTED_URL");
    const result = JSON.parse(json);
    return result;
  }

  catcher.registerHandler(errorObj => {
    callBackground("reportError", sanitizeError(errorObj));
  });

  function hideUIFrame() {
    ui.iframe.hide();
    return Promise.resolve(null);
  }

  function screenshotPage(dataUrl, selectedPos, type, screenshotTaskFn) {
    let promise = Promise.resolve(dataUrl);

    if (!dataUrl) {
      let isFullPage = type === "fullPage" || type == "fullPageTruncated";
      promise = callBackground(
        "screenshotPage",
        selectedPos.toJSON(),
        isFullPage,
        window.devicePixelRatio
      );
    }

    catcher.watchPromise(
      promise.then(dataLoc => {
        screenshotTaskFn(dataLoc);
      })
    );
  }

  exports.downloadShot = function(selectedPos, previewDataUrl, type) {
    const shotPromise = previewDataUrl
      ? Promise.resolve(previewDataUrl)
      : hideUIFrame();
    catcher.watchPromise(
      shotPromise.then(dataUrl => {
        screenshotPage(dataUrl, selectedPos, type, url => {
          let typeFromDataUrl = blobConverters.getTypeFromDataUrl(url);
          typeFromDataUrl = typeFromDataUrl
            ? typeFromDataUrl.split("/", 2)[1]
            : null;
          shotObject.delAllClips();
          shotObject.addClip({
            createdDate: Date.now(),
            image: {
              url,
              type: typeFromDataUrl,
              location: selectedPos,
            },
          });
          ui.triggerDownload(url, shotObject.filename);
          uicontrol.deactivate();
        });
      })
    );
  };

  exports.preview = function(selectedPos, type) {
    catcher.watchPromise(
      hideUIFrame().then(dataUrl => {
        screenshotPage(dataUrl, selectedPos, type, url => {
          ui.iframe.usePreview();
          ui.Preview.display(url);
        });
      })
    );
  };

  let copyInProgress = null;
  exports.copyShot = function(selectedPos, previewDataUrl, type) {
    // This is pretty slow. We'll ignore additional user triggered copy events
    // while it is in progress.
    if (copyInProgress) {
      return;
    }
    // A max of five seconds in case some error occurs.
    copyInProgress = setTimeout(() => {
      copyInProgress = null;
    }, 5000);

    const unsetCopyInProgress = () => {
      if (copyInProgress) {
        clearTimeout(copyInProgress);
        copyInProgress = null;
      }
    };
    const shotPromise = previewDataUrl
      ? Promise.resolve(previewDataUrl)
      : hideUIFrame();
    catcher.watchPromise(
      shotPromise.then(dataUrl => {
        screenshotPage(dataUrl, selectedPos, type, url => {
          const blob = blobConverters.dataUrlToBlob(url);
          catcher.watchPromise(
            callBackground("copyShotToClipboard", blob).then(() => {
              uicontrol.deactivate();
              unsetCopyInProgress();
            }, unsetCopyInProgress)
          );
        });
      })
    );
  };

  exports.sendEvent = function(...args) {
    const maybeOptions = args[args.length - 1];

    if (typeof maybeOptions === "object") {
      maybeOptions.incognito = browser.extension.inIncognitoContext;
    } else {
      args.push({ incognito: browser.extension.inIncognitoContext });
    }

    callBackground("sendEvent", ...args);
  };

  catcher.watchFunction(() => {
    shotObject = new AbstractShot(
      backend,
      randomString(RANDOM_STRING_LENGTH) + "/" + domainFromUrl(location),
      {
        origin: shot.originFromUrl(location.href),
      }
    );
    shotObject.update(documentMetadata());
  })();

  return exports;
})();
null;
