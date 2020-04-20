/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
SimpleTest.requestFlakyTimeout(
  "testing mozbrowser data: navigation is blocked"
);

browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

// make sure top level data: URI navigations are blocked.
const PREF = "security.data_uri.block_toplevel_data_uri_navigations";
browserElementTestHelpers._setPref(PREF, true);

const INNER = "foo";
const DATA_URI = "data:text/html,<html><body>" + INNER + "</body></html>";
const HTTP_URI = "browserElement_DataURILoad.html";

function runTest1() {
  let frame = document.createElement("iframe");
  frame.setAttribute("mozbrowser", "true");
  frame.src = DATA_URI;
  document.body.appendChild(frame);
  let wrappedFrame = SpecialPowers.wrap(frame);

  // wait for 1000ms and check that the data: URI did not load
  setTimeout(function() {
    isnot(
      wrappedFrame.contentWindow.document.body.innerHTML,
      INNER,
      "data: URI navigation should be blocked"
    );
    runTest2();
  }, 1000);
}

function runTest2() {
  let frame = document.createElement("iframe");
  frame.setAttribute("mozbrowser", "true");
  frame.src = HTTP_URI;
  document.body.appendChild(frame);
  let wrappedFrame = SpecialPowers.wrap(frame);

  wrappedFrame.addEventListener(
    "mozbrowserloadend",
    function onloadend(e) {
      ok(
        wrappedFrame.contentWindow.document.location.href.endsWith(HTTP_URI),
        "http: URI navigation should be allowed"
      );
      frame.src = DATA_URI;

      // wait for 1000ms and check that the data: URI did not load
      setTimeout(function() {
        isnot(
          wrappedFrame.contentWindow.document.body.innerHTML,
          INNER,
          "data: URI navigation should be blocked"
        );
        SimpleTest.finish();
      }, 1000);
    },
    { once: true }
  );
}

addEventListener("testready", runTest1);
