/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 795317: Test that the browser element sanitizes its URIs by removing the
// "unexposable" parts before sending them in the locationchange event.

"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframe;

function testPassword() {
  function locationchange(e) {
    var uri = e.detail.url;
    is(
      uri,
      "http://mochi.test:8888/tests/dom/browser-element/mochitest/file_empty.html",
      "Username and password shouldn't be exposed in uri."
    );
    SimpleTest.finish();
  }

  iframe.addEventListener("mozbrowserlocationchange", locationchange);
  iframe.src =
    "http://iamuser:iampassword@mochi.test:8888/tests/dom/browser-element/mochitest/file_empty.html";
}

function runTest() {
  SpecialPowers.pushPrefEnv(
    { set: [["network.http.rcwn.enabled", false]] },
    _ => {
      iframe = document.createElement("iframe");
      iframe.setAttribute("mozbrowser", "true");
      document.body.appendChild(iframe);
      testPassword();
    }
  );
}

addEventListener("testready", runTest);
