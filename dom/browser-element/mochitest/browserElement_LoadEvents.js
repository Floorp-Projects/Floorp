/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that an iframe with the |mozbrowser| attribute emits mozbrowserloadX
// events when this page is in the whitelist.

"use strict";
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function runTest() {
  // Load emptypage1 into the iframe, wait for that to finish loading, then
  // call runTest2.
  //
  // This should trigger loadstart, locationchange, loadprogresschanged  and
  // loadend events.

  var seenLoadEnd = false;
  var seenLoadStart = false;
  var seenLocationChange = false;

  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  iframe.id = 'iframe';
  iframe.src = 'http://example.com/tests/dom/browser-element/mochitest/file_browserElement_LoadEvents.html';

  function loadstart(e) {
    ok(e.isTrusted, 'Event should be trusted.');
    ok(!seenLoadEnd, 'loadstart before loadend.');
    ok(!seenLoadStart, 'Just one loadstart event.');
    ok(!seenLocationChange, 'loadstart before locationchange.');
    seenLoadStart = true;
  }

  function locationchange(e) {
    ok(e.isTrusted, 'Event should be trusted.');
    ok(!seenLocationChange, 'Just one locationchange event.');
    seenLocationChange = true;
    ok(seenLoadStart, 'Location change after load start.');
    ok(!seenLoadEnd, 'Location change before load end.');
    ok(e.detail, browserElementTestHelpers.emptyPage1, "event's reported location");
  }

  function loadend(e) {
    ok(e.isTrusted, 'Event should be trusted.');
    ok(seenLoadStart, 'loadend after loadstart.');
    ok(!seenLoadEnd, 'Just one loadend event.');
    ok(seenLocationChange, 'loadend after locationchange.');
    is(e.detail.backgroundColor, 'rgb(0, 128, 0)', 'Expected background color reported')
    seenLoadEnd = true;
  }

  iframe.addEventListener('mozbrowserloadstart', loadstart);
  iframe.addEventListener('mozbrowserlocationchange', locationchange);
  iframe.addEventListener('mozbrowserloadend', loadend);

  function waitForAllCallbacks() {
    if (!seenLoadStart || !seenLoadEnd) {
      SimpleTest.executeSoon(waitForAllCallbacks);
      return;
    }

    iframe.removeEventListener('mozbrowserloadstart', loadstart);
    iframe.removeEventListener('mozbrowserlocationchange', locationchange);
    iframe.removeEventListener('mozbrowserloadend', loadend);
    runTest2();
  }

  document.body.appendChild(iframe);
  waitForAllCallbacks();
}

function runTest2() {
  var seenLoadStart = false;
  var seenLoadEnd = false;
  var seenLocationChange = false;
  var seenLoadProgressChanged = false;

  // Add this event listener to the document; the events should bubble.
  document.addEventListener('mozbrowserloadstart', function(e) {
    ok(e.isTrusted, 'Event should be trusted.');
    ok(!seenLoadStart, 'Just one loadstart event.');
    seenLoadStart = true;
    ok(!seenLoadEnd, 'Got mozbrowserloadstart before loadend.');
    ok(!seenLocationChange, 'Got mozbrowserloadstart before locationchange.');
  });

  var iframe = document.getElementById('iframe');
  iframe.addEventListener('mozbrowserlocationchange', function(e) {
    ok(e.isTrusted, 'Event should be trusted.');
    ok(!seenLocationChange, 'Just one locationchange event.');
    seenLocationChange = true;
    ok(seenLoadStart, 'Location change after load start.');
    ok(!seenLoadEnd, 'Location change before load end.');
    ok(e.detail, browserElementTestHelpers.emptyPage2, "event's reported location");
  });

  iframe.addEventListener('mozbrowserloadend', function(e) {
    ok(e.isTrusted, 'Event should be trusted.');
    ok(!seenLoadEnd, 'Just one load end event.');
    seenLoadEnd = true;
    ok(seenLoadStart, 'Load end after load start.');
    ok(seenLocationChange, 'Load end after location change.');
    is(e.detail.backgroundColor, 'transparent', 'Expected background color reported')
  });

  iframe.addEventListener('mozbrowserloadprogresschanged', function(e) {
    ok(e.isTrusted, 'Event should be trusted.');
    seenLoadProgressChanged = true;
    ok(seenLoadStart, 'Load end after load start.');
    ok(seenLocationChange, 'Load end after location change.');
    ok(!seenLoadEnd, 'Load end after load progress.');
  });

  iframe.src = browserElementTestHelpers.emptyPage2;

  function waitForAllCallbacks() {
    if (!seenLoadStart || !seenLoadEnd || !seenLocationChange) {
      SimpleTest.executeSoon(waitForAllCallbacks);
      return;
    }

    SimpleTest.finish();
  }

  waitForAllCallbacks();
}

addEventListener('testready', runTest);
