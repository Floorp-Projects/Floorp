/* globals analytics, communication, makeUuid, Raven, catcher, auth, log */

"use strict";

const startTime = Date.now();

this.senderror = (function() {
  let exports = {};

  let manifest = browser.runtime.getManifest();

  // Do not show an error more than every ERROR_TIME_LIMIT milliseconds:
  const ERROR_TIME_LIMIT = 3000;

  let messages = {
    REQUEST_ERROR: {
      title: browser.i18n.getMessage("requestErrorTitle"),
      info: browser.i18n.getMessage("requestErrorDetails")
    },
    CONNECTION_ERROR: {
      title: browser.i18n.getMessage("connectionErrorTitle"),
      info: browser.i18n.getMessage("connectionErrorDetails")
    },
    LOGIN_ERROR: {
      title: browser.i18n.getMessage("requestErrorTitle"),
      info: browser.i18n.getMessage("loginErrorDetails")
    },
    LOGIN_CONNECTION_ERROR: {
      title: browser.i18n.getMessage("connectionErrorTitle"),
      info: browser.i18n.getMessage("connectionErrorDetails")
    },
    UNSHOOTABLE_PAGE: {
      title: browser.i18n.getMessage("unshootablePageErrorTitle"),
      info: browser.i18n.getMessage("unshootablePageErrorDetails")
    },
    SHOT_PAGE: {
      title: browser.i18n.getMessage("selfScreenshotErrorTitle")
    },
    MY_SHOTS: {
      title: browser.i18n.getMessage("selfScreenshotErrorTitle")
    },
    EMPTY_SELECTION: {
      title: browser.i18n.getMessage("emptySelectionErrorTitle")
    },
    PRIVATE_WINDOW: {
      title: browser.i18n.getMessage("privateWindowErrorTitle"),
      info: browser.i18n.getMessage("privateWindowErrorDetails")
    },
    generic: {
      title: browser.i18n.getMessage("genericErrorTitle"),
      info: browser.i18n.getMessage("genericErrorDetails"),
      showMessage: true
    }
  };

  communication.register("reportError", (sender, error) => {
    catcher.unhandled(error);
  });

  let lastErrorTime;

  exports.showError = function(error) {
    if (lastErrorTime && (Date.now() - lastErrorTime) < ERROR_TIME_LIMIT) {
      return;
    }
    lastErrorTime = Date.now();
    let id = makeUuid();
    let popupMessage = error.popupMessage || "generic";
    if (!messages[popupMessage]) {
      popupMessage = "generic";
    }
    let title = messages[popupMessage].title;
    let message = messages[popupMessage].info || '';
    let showMessage = messages[popupMessage].showMessage;
    if (error.message && showMessage) {
      if (message) {
        message += "\n" + error.message;
      } else {
        message = error.message;
      }
    }
    if (Date.now() - startTime > 5 * 1000) {
      browser.notifications.create(id, {
        type: "basic",
        // FIXME: need iconUrl for an image, see #2239
        title,
        message
      });
    }
  };

  exports.reportError = function(e) {
    if (!analytics.getTelemetryPrefSync()) {
      log.error("Telemetry disabled. Not sending critical error:", e);
      return;
    }
    let dsn = auth.getSentryPublicDSN();
    if (!dsn) {
      log.warn("Screenshots error:", e);
      return;
    }
    if (!Raven.isSetup()) {
      Raven.config(dsn, {allowSecretKey: true}).install();
    }
    let exception = new Error(e.message);
    exception.stack = e.multilineStack || e.stack || undefined;

    // To improve Sentry reporting & grouping, replace the
    // moz-extension://$uuid base URL with a generic resource:// URL.
    exception.stack = exception.stack.replace(
      /moz-extension:\/\/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}/g,
      "resource://screenshots-addon"
    );
    let rest = {};
    for (let attr in e) {
      if (!["name", "message", "stack", "multilineStack", "popupMessage", "version", "sentryPublicDSN", "help", "fromMakeError"].includes(attr)) {
        rest[attr] = e[attr];
      }
    }
    rest.stack = exception.stack;
    Raven.captureException(exception, {
      logger: 'addon',
      tags: {category: e.popupMessage},
      release: manifest.version,
      message: exception.message,
      extra: rest
    });
  };

  catcher.registerHandler((errorObj) => {
    if (!errorObj.noPopup) {
      exports.showError(errorObj);
    }
    if (!errorObj.noReport) {
      exports.reportError(errorObj);
    }
  });

  return exports;
})();
