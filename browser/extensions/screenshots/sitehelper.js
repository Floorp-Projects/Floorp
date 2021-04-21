/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals catcher, callBackground, content */
/** This is a content script added to all screenshots.firefox.com pages, and allows the site to
    communicate with the add-on */

"use strict";

this.sitehelper = (function() {
  // This gives us the content's copy of XMLHttpRequest, instead of the wrapped
  // copy that this content script gets:
  const ContentXMLHttpRequest = content.XMLHttpRequest;

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

  /** Set the cookie, even if third-party cookies are disabled in this browser
      (when they are disabled, login from the background page won't set cookies) */
  function sendBackupCookieRequest(authHeaders) {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1295660
    //   This bug would allow us to access window.content.XMLHttpRequest, and get
    //   a safer (not overridable by content) version of the object.

    // This is a very minimal attempt to verify that the XMLHttpRequest object we got
    // is legitimate. It is not a good test.
    if (
      Object.toString.apply(ContentXMLHttpRequest) !==
      "function XMLHttpRequest() {\n    [native code]\n}"
    ) {
      console.warn("Insecure copy of XMLHttpRequest");
      return;
    }
    const req = new ContentXMLHttpRequest();
    req.open("POST", "/api/set-login-cookie");
    for (const name in authHeaders) {
      req.setRequestHeader(name, authHeaders[name]);
    }
    req.send("");
    req.onload = () => {
      if (req.status !== 200) {
        console.warn(
          "Attempt to set Screenshots cookie via /api/set-login-cookie failed:",
          req.status,
          req.statusText,
          req.responseText
        );
      }
    };
  }

  registerListener(
    "delete-everything",
    catcher.watchFunction(event => {
      // FIXME: reset some data in the add-on
    }, false)
  );

  registerListener(
    "request-login",
    catcher.watchFunction(event => {
      const shotId = event.detail;
      catcher.watchPromise(
        callBackground("getAuthInfo", shotId || null).then(info => {
          if (info) {
            sendBackupCookieRequest(info.authHeaders);
            sendCustomEvent("login-successful", {
              accountId: info.accountId,
              isOwner: info.isOwner,
              backupCookieRequest: true,
            });
          }
        })
      );
    })
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
