/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals startBackground, analytics, communication, makeUuid, Raven, catcher, auth, log, browser, getStrings */

"use strict";

this.senderror = (function() {
  const exports = {};

  const manifest = browser.runtime.getManifest();

  // Do not show an error more than every ERROR_TIME_LIMIT milliseconds:
  const ERROR_TIME_LIMIT = 3000;

  const messages = {
    REQUEST_ERROR: {
      titleKey: "screenshots-request-error-title",
      infoKey: "screenshots-request-error-details",
    },
    CONNECTION_ERROR: {
      titleKey: "screenshots-connection-error-title",
      infoKey: "screenshots-connection-error-details",
    },
    LOGIN_ERROR: {
      titleKey: "screenshots-request-error-title",
      infoKey: "screenshots-login-error-details",
    },
    LOGIN_CONNECTION_ERROR: {
      titleKey: "screenshots-connection-error-title",
      infoKey: "screenshots-connection-error-details",
    },
    UNSHOOTABLE_PAGE: {
      titleKey: "screenshots-unshootable-page-error-title",
      infoKey: "screenshots-unshootable-page-error-details",
    },
    EMPTY_SELECTION: {
      titleKey: "screenshots-empty-selection-error-title",
    },
    PRIVATE_WINDOW: {
      titleKey: "screenshots-private-window-error-title",
      infoKey: "screenshots-private-window-error-details",
    },
    generic: {
      titleKey: "screenshots-generic-error-title",
      infoKey: "screenshots-generic-error-details",
      showMessage: true,
    },
  };

  communication.register("reportError", (sender, error) => {
    catcher.unhandled(error);
  });

  let lastErrorTime;

  exports.showError = async function(error) {
    if (lastErrorTime && Date.now() - lastErrorTime < ERROR_TIME_LIMIT) {
      return;
    }
    lastErrorTime = Date.now();
    const id = makeUuid();
    let popupMessage = error.popupMessage || "generic";
    if (!messages[popupMessage]) {
      popupMessage = "generic";
    }

    let item = messages[popupMessage];
    if (!("title" in item)) {
      let keys = [{ id: item.titleKey }];
      if ("infoKey" in item) {
        keys.push({ id: item.infoKey });
      }

      [item.title, item.info] = await getStrings(keys);
    }

    let title = item.title;
    let message = item.info || "";
    const showMessage = item.showMessage;
    if (error.message && showMessage) {
      if (message) {
        message += "\n" + error.message;
      } else {
        message = error.message;
      }
    }
    if (Date.now() - startBackground.startTime > 5 * 1000) {
      browser.notifications.create(id, {
        type: "basic",
        // FIXME: need iconUrl for an image, see #2239
        title,
        message,
      });
    }
  };

  exports.reportError = function(e) {
    if (!analytics.isTelemetryEnabled()) {
      log.error("Telemetry disabled. Not sending critical error:", e);
      return;
    }
    const dsn = auth.getSentryPublicDSN();
    if (!dsn) {
      return;
    }
    if (!Raven.isSetup()) {
      Raven.config(dsn, { allowSecretKey: true }).install();
    }
    const exception = new Error(e.message);
    exception.stack = e.multilineStack || e.stack || undefined;

    // To improve Sentry reporting & grouping, replace the
    // moz-extension://$uuid base URL with a generic resource:// URL.
    if (exception.stack) {
      exception.stack = exception.stack.replace(
        /moz-extension:\/\/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}/g,
        "resource://screenshots-addon"
      );
    }
    const rest = {};
    for (const attr in e) {
      if (
        ![
          "name",
          "message",
          "stack",
          "multilineStack",
          "popupMessage",
          "version",
          "sentryPublicDSN",
          "help",
          "fromMakeError",
        ].includes(attr)
      ) {
        rest[attr] = e[attr];
      }
    }
    rest.stack = exception.stack;
    Raven.captureException(exception, {
      logger: "addon",
      tags: { category: e.popupMessage },
      release: manifest.version,
      message: exception.message,
      extra: rest,
    });
  };

  catcher.registerHandler(errorObj => {
    if (!errorObj.noPopup) {
      exports.showError(errorObj);
    }
    if (!errorObj.noReport) {
      exports.reportError(errorObj);
    }
  });

  return exports;
})();
