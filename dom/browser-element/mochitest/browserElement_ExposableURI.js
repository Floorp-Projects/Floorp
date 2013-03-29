/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 795317: Test that the browser element sanitizes its URIs by removing the
// "unexposable" parts before sending them in the locationchange event.

"use strict";
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframe;

function testPassword() {
  function locationchange(e) {
    var uri = e.detail;
    is(uri, 'http://mochi.test:8888/tests/dom/browser-element/mochitest/file_empty.html',
       "Username and password shouldn't be exposed in uri.");
    SimpleTest.finish();
  }

  iframe.addEventListener('mozbrowserlocationchange', locationchange);
  iframe.src = "http://iamuser:iampassword@mochi.test:8888/tests/dom/browser-element/mochitest/file_empty.html";
}

function testWyciwyg() {
  var locationChangeCount = 0;

  function locationchange(e) {
    // locationChangeCount:
    //  0 - the first load.
    //  1 - after document.write().
    if (locationChangeCount == 0) {
      locationChangeCount ++;
    } else if (locationChangeCount == 1) {
      var uri = e.detail;
      is(uri, 'http://mochi.test:8888/tests/dom/browser-element/mochitest/file_wyciwyg.html', "Scheme in string shouldn't be wyciwyg");
      iframe.removeEventListener('mozbrowserlocationchange', locationchange);
      SimpleTest.executeSoon(testPassword);
    }
  }

  // file_wyciwyg.html calls document.write() to create a wyciwyg channel.
  iframe.src = 'file_wyciwyg.html';
  iframe.addEventListener('mozbrowserlocationchange', locationchange);
}

function runTest() {
  iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;
  document.body.appendChild(iframe);
  testWyciwyg();
}

addEventListener('testready', runTest);
