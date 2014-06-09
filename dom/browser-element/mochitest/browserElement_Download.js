/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 983747 - Test 'download' method on iframe.

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframe;
var downloadURL = 'http://test/tests/dom/browser-element/mochitest/file_download_bin.sjs';

function runTest() {
  iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;

  iframe.addEventListener('mozbrowserloadend', loadend);
  iframe.src = 'data:text/html,<html><body>hello</body></html>';
  iframe.setAttribute('remote', 'true');

  document.body.appendChild(iframe);
}

function loadend() {
  var req = iframe.download(downloadURL, { filename: 'test.bin' });
  req.onsuccess = function() {
    ok(true, 'Download finished as expected.');
    SimpleTest.finish();
  }
  req.onerror = function() {
    ok(false, 'Expected no error, got ' + req.error);
  }
}

addEventListener('testready', runTest);
