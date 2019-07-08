/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 741717 - Test the reload ability of <iframe mozbrowser>.

"use strict";

/* global browserElementTestHelpers */
/* eslint-env mozilla/frame-script */

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframeScript = function() {
  sendAsyncMessage("test:innerHTML", {
    data: XPCNativeWrapper.unwrap(content).document.body.innerHTML,
  });
};

var mm;
var iframe;
var loadedEvents = 0;
var countAcc;

function runTest() {
  iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "true");

  iframe.addEventListener("mozbrowserloadend", mozbrowserLoaded);

  iframe.src = "file_bug741717.sjs";
  document.body.appendChild(iframe);
}

function iframeBodyRecv(data) {
  data = SpecialPowers.wrap(data);
  var previousCount = countAcc;
  var currentCount = parseInt(data.json.data, 10);
  countAcc = currentCount;
  switch (loadedEvents) {
    case 1:
      iframe.reload();
      break;
    case 2:
      ok(true, "reload was triggered");
      ok(previousCount === currentCount, "reload was a soft reload");
      iframe.reload(true);
      break;
    case 3:
      ok(currentCount > previousCount, "reload was a hard reload");
      SimpleTest.finish();
  }
}

function mozbrowserLoaded() {
  loadedEvents++;
  mm = SpecialPowers.getBrowserFrameMessageManager(iframe);
  mm.addMessageListener("test:innerHTML", iframeBodyRecv);
  mm.loadFrameScript("data:,(" + iframeScript.toString() + ")();", false);
}

addEventListener("testready", runTest);
