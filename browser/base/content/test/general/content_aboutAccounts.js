/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded as a "content script" for browser_aboutAccounts tests
"use strict";

/* eslint-env mozilla/frame-script */

var {interfaces: Ci, utils: Cu} = Components;

addEventListener("load", function load(event) {
  if (event.target != content.document) {
    return;
  }
//  content.document.removeEventListener("load", load, true);
  sendAsyncMessage("test:document:load");
  // Opening Sync prefs in tests is a pain as leaks are reported due to the
  // in-flight promises. For now we just mock the openPrefs() function and have
  // it send a message back to the test so we know it was called.
  content.openPrefs = function() {
    sendAsyncMessage("test:openPrefsCalled");
  }
}, true);

addEventListener("DOMContentLoaded", function domContentLoaded(event) {
  removeEventListener("DOMContentLoaded", domContentLoaded, true);
  let iframe = content.document.getElementById("remote");
  if (!iframe) {
    // at least one test initially loads about:blank - in that case, we are done.
    return;
  }
  // We use DOMContentLoaded here as that fires for our iframe even when we've
  // arranged for the URL in the iframe to cause an error.
  addEventListener("DOMContentLoaded", function iframeLoaded(dclEvent) {
    if (iframe.contentWindow.location.href == "about:blank" ||
        dclEvent.target != iframe.contentDocument) {
      return;
    }
    removeEventListener("DOMContentLoaded", iframeLoaded, true);
    sendAsyncMessage("test:iframe:load", {url: iframe.contentDocument.location.href});
    // And an event listener for the test responses, which we send to the test
    // via a message.
    iframe.contentWindow.addEventListener("FirefoxAccountsTestResponse", function(fxAccountsEvent) {
      sendAsyncMessage("test:response", {data: fxAccountsEvent.detail.data});
    }, true);
  }, true);
}, true);

// Return the visibility state of a list of ids.
addMessageListener("test:check-visibilities", function(message) {
  let result = {};
  for (let id of message.data.ids) {
    let elt = content.document.getElementById(id);
    if (elt) {
      let displayStyle = content.window.getComputedStyle(elt).display;
      if (displayStyle == "none") {
        result[id] = false;
      } else if (displayStyle == "block") {
        result[id] = true;
      } else {
        result[id] = "strange: " + displayStyle; // tests should fail!
      }
    } else {
      result[id] = "doesn't exist: " + id;
    }
  }
  sendAsyncMessage("test:check-visibilities-response", result);
});

addMessageListener("test:load-with-mocked-profile-path", function(message) {
  addEventListener("DOMContentLoaded", function domContentLoaded(event) {
    removeEventListener("DOMContentLoaded", domContentLoaded, true);
    content.getDefaultProfilePath = () => message.data.profilePath;
    // now wait for the iframe to load.
    let iframe = content.document.getElementById("remote");
    iframe.addEventListener("load", function iframeLoaded(loadEvent) {
      if (iframe.contentWindow.location.href == "about:blank" ||
          loadEvent.target != iframe) {
        return;
      }
      iframe.removeEventListener("load", iframeLoaded, true);
      sendAsyncMessage("test:load-with-mocked-profile-path-response",
                       {url: iframe.contentDocument.location.href});
    }, true);
  });
  let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
  webNav.loadURI(message.data.url, webNav.LOAD_FLAGS_NONE, null, null, null);
}, true);
