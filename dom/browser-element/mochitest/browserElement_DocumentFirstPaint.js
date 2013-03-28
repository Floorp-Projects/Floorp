/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 829486 - Add mozdocumentbrowserfirstpaint event.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframe;

function runTestQueue(queue) {
  if (queue.length == 0) {
    SimpleTest.finish();
    return;
  }

  var gotFirstPaint = false;
  var gotFirstLocationChange = false;
  var test = queue.shift();

  function runNext() {
    iframe.removeEventListener('mozbrowserdocumentfirstpaint', documentfirstpainthandler);
    iframe.removeEventListener('mozbrowserloadend', loadendhandler);
    runTestQueue(queue);
  }

  function documentfirstpainthandler(e) {
    ok(!gotFirstPaint, "Got firstpaint only once");
    gotFirstPaint = true;
    if (gotFirstLocationChange) {
      runNext();
    }
  }

  function loadendhandler(e) {
    gotFirstLocationChange = true;
    if (gotFirstPaint) {
      runNext();
    }
  }

  iframe.addEventListener('mozbrowserdocumentfirstpaint', documentfirstpainthandler);
  iframe.addEventListener('mozbrowserloadend', loadendhandler);

  test();
}

function testChangeLocation() {
  iframe.src = browserElementTestHelpers.emptyPage1 + "?2";
}

function testReload() {
  iframe.reload();
}

function testFirstLoad() {
  document.body.appendChild(iframe);
  iframe.src = browserElementTestHelpers.emptyPage1;
}

function runTest() {
  iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;

  runTestQueue([testFirstLoad, testReload, testChangeLocation]);
}

addEventListener('testready', runTest);
