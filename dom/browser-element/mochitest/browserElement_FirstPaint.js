/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 787378 - Add mozbrowserfirstpaint event.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  var iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;

  var gotFirstPaint = false;
  var gotFirstLocationChange = false;
  iframe.addEventListener('mozbrowserfirstpaint', function(e) {
    ok(!gotFirstPaint, "Got only one first paint.");
    gotFirstPaint = true;

    if (gotFirstLocationChange) {
      iframe.src = browserElementTestHelpers.emptyPage1 + '?2';
    }
  });

  iframe.addEventListener('mozbrowserlocationchange', function(e) {
    if (e.detail == browserElementTestHelpers.emptyPage1) {
      gotFirstLocationChange = true;
      if (gotFirstPaint) {
        iframe.src = browserElementTestHelpers.emptyPage1 + '?2';
      }
    }
    else if (e.detail.endsWith('?2')) {
      SimpleTest.finish();
    }
  });

  document.body.appendChild(iframe);

  iframe.src = browserElementTestHelpers.emptyPage1;
}

addEventListener('testready', runTest);
