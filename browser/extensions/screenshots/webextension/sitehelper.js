/* globals catcher, callBackground */
/** This is a content script added to all screenshots.firefox.com pages, and allows the site to
    communicate with the add-on */

"use strict";

this.sitehelper = (function() {

  let ContentXMLHttpRequest = XMLHttpRequest;
  // This gives us the content's copy of XMLHttpRequest, instead of the wrapped
  // copy that this content script gets:
  if (location.origin === "https://screenshots.firefox.com" ||
      location.origin === "http://localhost:10080") {
    // Note http://localhost:10080 is the default development server
    // This code should always run, unless this content script is
    // somehow run in a bad/malicious context
    ContentXMLHttpRequest = window.wrappedJSObject.XMLHttpRequest;
  }

  catcher.registerHandler((errorObj) => {
    callBackground("reportError", errorObj);
  });


  function sendCustomEvent(name, detail) {
    if (typeof detail == "object") {
      // Note sending an object can lead to security problems, while a string
      // is safe to transfer:
      detail = JSON.stringify(detail);
    }
    document.dispatchEvent(new CustomEvent(name, {detail}));
  }

  /** Set the cookie, even if third-party cookies are disabled in this browser
      (when they are disabled, login from the background page won't set cookies) */
  function sendBackupCookieRequest(authHeaders) {
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1295660
    //   This bug would allow us to access window.content.XMLHttpRequest, and get
    //   a safer (not overridable by content) version of the object.

    // This is a very minimal attempt to verify that the XMLHttpRequest object we got
    // is legitimate. It is not a good test.
    if (Object.toString.apply(ContentXMLHttpRequest) != "function XMLHttpRequest() {\n    [native code]\n}") {
      console.warn("Insecure copy of XMLHttpRequest");
      return;
    }
    let req = new ContentXMLHttpRequest();
    req.open("POST", "/api/set-login-cookie");
    for (let name in authHeaders) {
      req.setRequestHeader(name, authHeaders[name]);
    }
    req.send("");
    req.onload = () => {
      if (req.status != 200) {
        console.warn("Attempt to set Screenshots cookie via /api/set-login-cookie failed:", req.status, req.statusText, req.responseText);
      }
    };
  }

  document.addEventListener("delete-everything", catcher.watchFunction((event) => {
    // FIXME: reset some data in the add-on
  }, false));

  document.addEventListener("request-login", catcher.watchFunction((event) => {
    let shotId = event.detail;
    catcher.watchPromise(callBackground("getAuthInfo", shotId || null).then((info) => {
      sendBackupCookieRequest(info.authHeaders);
      sendCustomEvent("login-successful", {deviceId: info.deviceId, isOwner: info.isOwner});
    }));
  }));

  document.addEventListener("request-onboarding", catcher.watchFunction((event) => {
    callBackground("requestOnboarding");
  }));

  // Depending on the script loading order, the site might get the addon-present event,
  // but probably won't - instead the site will ask for that event after it has loaded
  document.addEventListener("request-addon-present", catcher.watchFunction(() => {
    sendCustomEvent("addon-present");
  }));

  sendCustomEvent("addon-present");

})();
null;
