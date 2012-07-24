/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 771273 - Check that window.open(url, '_top') works properly with <iframe
// mozbrowser>.
"use strict";

SimpleTest.waitForExplicitFinish();

function runTest() {
  browserElementTestHelpers.setEnabledPref(true);
  browserElementTestHelpers.addToWhitelist();

  var iframe = document.createElement('iframe');
  iframe.mozbrowser = true;

  iframe.addEventListener('mozbrowseropenwindow', function(e) {
    ok(false, 'Not expecting an openwindow event.');
  });

  iframe.addEventListener('mozbrowserlocationchange', function(e) {
    if (/file_browserElement_TargetTop.html\?2$/.test(e.detail)) {
      ok(true, 'Got the locationchange we were looking for.');
      SimpleTest.finish();
    }
  });

  document.body.appendChild(iframe);
  iframe.src = 'file_browserElement_TargetTop.html';
}

runTest();
