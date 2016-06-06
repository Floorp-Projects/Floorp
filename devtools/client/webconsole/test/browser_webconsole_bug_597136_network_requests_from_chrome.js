/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that network requests from chrome don't cause the Web Console to
// throw exceptions.

"use strict";

const TEST_URI = "http://example.com/";

var good = true;
var listener = {
  QueryInterface: XPCOMUtils.generateQI([ Ci.nsIObserver ]),
  observe: function (subject) {
    if (subject instanceof Ci.nsIScriptError &&
        subject.category === "XPConnect JavaScript" &&
        subject.sourceName.includes("webconsole")) {
      good = false;
    }
  }
};

var xhr;

function test() {
  Services.console.registerListener(listener);

  // trigger a lazy-load of the HUD Service
  HUDService;

  xhr = new XMLHttpRequest();
  xhr.addEventListener("load", xhrComplete, false);
  xhr.open("GET", TEST_URI, true);
  xhr.send(null);
}

function xhrComplete() {
  xhr.removeEventListener("load", xhrComplete, false);
  window.setTimeout(checkForException, 0);
}

function checkForException() {
  ok(good, "no exception was thrown when sending a network request from a " +
     "chrome window");

  Services.console.unregisterListener(listener);
  listener = xhr = null;

  finishTest();
}
