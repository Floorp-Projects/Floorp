/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals catcher, callBackground, content */
/** This is a content script added to all screenshots.firefox.com pages, and allows the site to
    communicate with the add-on */

"use strict";

this.sitehelper = (function () {
  catcher.registerHandler(errorObj => {
    callBackground("reportError", errorObj);
  });

  const capabilities = {};
  function registerListener(name, func) {
    capabilities[name] = name;
    document.addEventListener(name, func);
  }

  function sendCustomEvent(name, detail) {
    if (typeof detail === "object") {
      // Note sending an object can lead to security problems, while a string
      // is safe to transfer:
      detail = JSON.stringify(detail);
    }
    document.dispatchEvent(new CustomEvent(name, { detail }));
  }

  registerListener(
    "delete-everything",
    catcher.watchFunction(event => {
      // FIXME: reset some data in the add-on
    }, false)
  );

  registerListener(
    "copy-to-clipboard",
    catcher.watchFunction(event => {
      catcher.watchPromise(callBackground("copyShotToClipboard", event.detail));
    })
  );

  registerListener(
    "show-notification",
    catcher.watchFunction(event => {
      catcher.watchPromise(callBackground("showNotification", event.detail));
    })
  );

  // Depending on the script loading order, the site might get the addon-present event,
  // but probably won't - instead the site will ask for that event after it has loaded
  registerListener(
    "request-addon-present",
    catcher.watchFunction(() => {
      sendCustomEvent("addon-present", capabilities);
    })
  );

  sendCustomEvent("addon-present", capabilities);
})();
null;
