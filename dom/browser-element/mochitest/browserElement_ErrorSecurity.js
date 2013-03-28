/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 764718 - Test that mozbrowsererror works for a security error.

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;

  iframe.addEventListener("mozbrowsererror", function(e) {
    ok(true, "Got mozbrowsererror event.");
    ok(e.detail.type, "Event's detail has a |type| param.");
    SimpleTest.finish();
  });

  iframe.src = "https://expired.example.com";
  document.body.appendChild(iframe);
}

addEventListener('testready', runTest);
