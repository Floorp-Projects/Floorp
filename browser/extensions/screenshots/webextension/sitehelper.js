/* globals catcher, callBackground */
/** This is a content script added to all screenshots.firefox.com pages, and allows the site to
    communicate with the add-on */

"use strict";

this.sitehelper = (function() {

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

  document.addEventListener("delete-everything", catcher.watchFunction((event) => {
    // FIXME: reset some data in the add-on
  }, false));

  document.addEventListener("request-login", catcher.watchFunction((event) => {
    let shotId = event.detail;
    catcher.watchPromise(callBackground("getAuthInfo", shotId || null).then((info) => {
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
