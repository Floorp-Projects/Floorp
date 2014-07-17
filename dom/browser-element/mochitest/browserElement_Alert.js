/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that alert works.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var numPendingChildTests = 0;
var iframe;
var mm;

function runTest() {
  iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;
  document.body.appendChild(iframe);

  mm = SpecialPowers.getBrowserFrameMessageManager(iframe);
  mm.addMessageListener('test-success', function(msg) {
    numPendingChildTests--;
    ok(true, SpecialPowers.wrap(msg).json);
  });
  mm.addMessageListener('test-fail', function(msg) {
    numPendingChildTests--;
    ok(false, SpecialPowers.wrap(msg).json);
  });

  // Wait for the initial load to finish, then navigate the page, then wait
  // for that load to finish, then start test1.
  iframe.addEventListener('mozbrowserloadend', function loadend() {
    iframe.removeEventListener('mozbrowserloadend', loadend);
    iframe.src = browserElementTestHelpers.emptyPage1;

    iframe.addEventListener('mozbrowserloadend', function loadend2() {
      iframe.removeEventListener('mozbrowserloadend', loadend2);
      SimpleTest.executeSoon(test1);
    });
  });

}

function test1() {
  iframe.addEventListener('mozbrowsershowmodalprompt', test2);

  // Do window.alert within the iframe, then modify the global |testState|
  // after the alert.
  var script = 'data:,\
    this.testState = 0; \
    content.alert("Hello, world!"); \
    this.testState = 1; \
  ';

  mm.loadFrameScript(script, /* allowDelayedLoad = */ false);

  // Triggers a mozbrowsershowmodalprompt event, which sends us down to test2.
}

// test2 is a mozbrowsershowmodalprompt listener.
function test2(e) {
  iframe.removeEventListener("mozbrowsershowmodalprompt", test2);

  is(e.detail.message, 'Hello, world!');
  e.preventDefault(); // cause the alert to block.

  SimpleTest.executeSoon(function() { test2a(e); });
}

function test2a(e) {
  // The iframe should be blocked on the alert call at the moment, so testState
  // should still be 0.
  var script = 'data:,\
    if (this.testState === 0) { \
      sendAsyncMessage("test-success", "1: Correct testState"); \
    } \
    else { \
      sendAsyncMessage("test-fail", "1: Wrong testState: " + this.testState); \
    }';

  mm.loadFrameScript(script, /* allowDelayedLoad = */ false);
  numPendingChildTests++;

  waitForPendingTests(function() { test3(e); });
}

function test3(e) {
  // Now unblock the iframe and check that the script completed.
  e.detail.unblock();

  var script2 = 'data:,\
    if (this.testState === 1) { \
      sendAsyncMessage("test-success", "2: Correct testState"); \
    } \
    else { \
      sendAsyncMessage("test-try-again", "2: Wrong testState (for now): " + this.testState); \
    }';

  // Urgh.  e.unblock() didn't necessarily unblock us immediately, so we have
  // to spin and wait.
  function onTryAgain() {
    SimpleTest.executeSoon(function() {
      //dump('onTryAgain\n');
      mm.loadFrameScript(script2, /* allowDelayedLoad = */ false);
    });
  }

  mm.addMessageListener('test-try-again', onTryAgain);
  numPendingChildTests++;

  onTryAgain();
  waitForPendingTests(function() { test4(); });
}

function test4() {
  // Navigate the iframe while an alert is pending.  This shouldn't screw
  // things up.

  iframe.addEventListener("mozbrowsershowmodalprompt", test5);

  var script = 'data:,content.alert("test4");';
  mm.loadFrameScript(script, /* allowDelayedLoad = */ false);
}

// test4 is a mozbrowsershowmodalprompt listener.
function test5(e) {
  iframe.removeEventListener('mozbrowsershowmodalprompt', test4);

  is(e.detail.message, 'test4');
  e.preventDefault(); // cause the page to block.

  SimpleTest.executeSoon(test5a);
}

function test5a() {
  iframe.addEventListener('mozbrowserloadend', test5b);
  iframe.src = browserElementTestHelpers.emptyPage2;
}

function test5b() {
  iframe.removeEventListener('mozbrowserloadend', test5b);
  SimpleTest.finish();
}

var prevNumPendingTests = null;
function waitForPendingTests(next) {
  if (numPendingChildTests !== prevNumPendingTests) {
    dump("Waiting for end; " + numPendingChildTests + " pending tests\n");
    prevNumPendingTests = numPendingChildTests;
  }

  if (numPendingChildTests > 0) {
    SimpleTest.executeSoon(function() { waitForPendingTests(next); });
    return;
  }

  prevNumPendingTests = null;
  next();
}

addEventListener('testready', runTest);
