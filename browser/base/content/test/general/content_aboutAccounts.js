/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded as a "content script" for browser_aboutAccounts tests
"use strict";

const {interfaces: Ci, utils: Cu} = Components;

addEventListener("load", function load(event) {
  if (event.target != content.document) {
    return;
  }
//  content.document.removeEventListener("load", load, true);
  sendAsyncMessage("test:document:load");
}, true);

addEventListener("DOMContentLoaded", function domContentLoaded(event) {
  removeEventListener("DOMContentLoaded", domContentLoaded, true);
  let iframe = content.document.getElementById("remote");
  if (!iframe) {
    // at least one test initially loads about:blank - in that case, we are done.
    return;
  }
  iframe.addEventListener("load", function iframeLoaded(event) {
    if (iframe.contentWindow.location.href == "about:blank" ||
        event.target != iframe) {
      return;
    }
    iframe.removeEventListener("load", iframeLoaded, true);
    sendAsyncMessage("test:iframe:load", {url: iframe.getAttribute("src")});
  }, true);
}, true);

// Return the visibility state of a list of ids.
addMessageListener("test:check-visibilities", function (message) {
  let result = {};
  for (let id of message.data.ids) {
    let elt = content.document.getElementById(id);
    if (elt) {
      let displayStyle = content.window.getComputedStyle(elt).display;
      if (displayStyle == 'none') {
        result[id] = false;
      } else if (displayStyle == 'block') {
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

addMessageListener("test:load-with-mocked-profile-path", function (message) {
  addEventListener("DOMContentLoaded", function domContentLoaded(event) {
    removeEventListener("DOMContentLoaded", domContentLoaded, true);
    content.getDefaultProfilePath = () => message.data.profilePath;
    // now wait for the iframe to load.
    let iframe = content.document.getElementById("remote");
    iframe.addEventListener("load", function iframeLoaded(event) {
      if (iframe.contentWindow.location.href == "about:blank" ||
          event.target != iframe) {
        return;
      }
      iframe.removeEventListener("load", iframeLoaded, true);
      sendAsyncMessage("test:load-with-mocked-profile-path-response",
                       {url: iframe.getAttribute("src")});
    }, true);
  });
  let webNav = docShell.QueryInterface(Ci.nsIWebNavigation);
  webNav.loadURI(message.data.url, webNav.LOAD_FLAGS_NONE, null, null, null);
}, true);
