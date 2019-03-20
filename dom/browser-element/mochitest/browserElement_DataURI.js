/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that data: URIs work with mozbrowserlocationchange events.

"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();
browserElementTestHelpers.allowTopLevelDataURINavigation();

function runTest() {
  var iframe1 = document.createElement("iframe");
  iframe1.setAttribute("mozbrowser", "true");
  iframe1.id = "iframe1";
  iframe1.addEventListener("mozbrowserloadend", function() {
    ok(true, "Got first loadend event.");
    SimpleTest.executeSoon(runTest2);
  }, {once: true});
  iframe1.src = browserElementTestHelpers.emptyPage1;
  document.body.appendChild(iframe1);

  var iframe2 = document.createElement("iframe");
  iframe2.id = "iframe2";
  document.body.appendChild(iframe2);
}

function runTest2() {
  var iframe1 = document.getElementById("iframe1");
  var iframe2 = document.getElementById("iframe2");

  var sawLoadEnd = false;
  var sawLocationChange = false;

  iframe1.addEventListener("mozbrowserlocationchange", function(e) {
    ok(e.isTrusted, "Event should be trusted.");
    ok(!sawLocationChange, "Just one locationchange event.");
    ok(!sawLoadEnd, "locationchange before load.");
    is(e.detail.url, "data:text/html,1", "event's reported location");
    sawLocationChange = true;
  });

  iframe1.addEventListener("mozbrowserloadend", function() {
    ok(sawLocationChange, "Loadend after locationchange.");
    ok(!sawLoadEnd, "Just one loadend event.");
    sawLoadEnd = true;
  });

  function iframe2Load() {
    if (!sawLoadEnd || !sawLocationChange) {
      // Spin if iframe1 hasn't loaded yet.
      SimpleTest.executeSoon(iframe2Load);
      return;
    }
    ok(true, "Got iframe2 load.");
    SimpleTest.finish();
  }
  iframe2.addEventListener("load", iframe2Load);


  iframe1.src = "data:text/html,1";

  // Load something into iframe2 to check that it doesn't trigger a
  // locationchange for our iframe1 listener.
  iframe2.src = browserElementTestHelpers.emptyPage2;
}

addEventListener("testready", runTest);
