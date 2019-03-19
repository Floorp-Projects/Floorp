/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 780351 - Test that mozbrowser does /not/ divide the window name namespace.
// Multiple mozbrowsers inside the same app are like multiple browser tabs;
// they share a window name namespace.

"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe1 = document.createElement("iframe");
  iframe1.setAttribute("mozbrowser", "true");

  // Two mozbrowser frames with the same code both do the same
  // window.open("foo", "bar") call.  We should only get one
  // mozbrowseropenwindow event.

  iframe1.addEventListener("mozbrowseropenwindow", function(e) {
    ok(true, "Got first mozbrowseropenwindow event.");
    document.body.appendChild(e.detail.frameElement);

    e.detail.frameElement.addEventListener("mozbrowserlocationchange", function(f) {
      if (f.detail.url == "http://example.com/#2") {
        ok(true, "Got locationchange to http://example.com/#2");
        SimpleTest.finish();
      } else {
        ok(true, "Got locationchange to " + f.detail.url);
      }
    });

    SimpleTest.executeSoon(function() {
      var iframe2 = document.createElement("iframe");
      // Make sure that iframe1 and iframe2 are in the same TabGroup by linking
      // them through opener. Right now this API requires chrome privileges, as
      // it is on MozFrameLoaderOwner.
      SpecialPowers.wrap(iframe2).presetOpenerWindow(iframe1.contentWindow);
      iframe2.setAttribute("mozbrowser", "true");

      iframe2.addEventListener("mozbrowseropenwindow", function(f) {
        ok(false, "Got second mozbrowseropenwindow event.");
      });

      document.body.appendChild(iframe2);
      iframe2.src = "file_browserElement_BrowserWindowNamespace.html#2";
    });
  });

  document.body.appendChild(iframe1);
  iframe1.src = "file_browserElement_BrowserWindowNamespace.html#1";
}

addEventListener("testready", runTest);
