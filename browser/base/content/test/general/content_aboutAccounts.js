/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded as a "content script" for browser_aboutAccounts tests
"use strict";

addEventListener("DOMContentLoaded", function domContentLoaded(event) {
  removeEventListener("DOMContentLoaded", domContentLoaded);
  sendAsyncMessage("test:document:load");
  let iframe = content.document.getElementById("remote");
  iframe.addEventListener("load", function iframeLoaded(event) {
    if (iframe.contentWindow.location.href == "about:blank" ||
        event.target != iframe) {
      return;
    }
    iframe.removeEventListener("load", iframeLoaded, true);
    sendAsyncMessage("test:iframe:load", {url: iframe.getAttribute("src")});
  }, true);
});
