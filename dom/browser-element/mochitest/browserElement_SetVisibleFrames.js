/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 762939 - Test that visibility propagates down properly through
// hierarchies of <iframe mozbrowser>.
//
// In this test, we modify the parent's visibility and check that the child's
// visibility is changed as appopriate.  We test modifying the child's
// visibility in a separate testcase.

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframe;

function runTest() {
  var principal = SpecialPowers.wrap(SpecialPowers.getNodePrincipal(document));
  SpecialPowers.addPermission("browser", true, { url: SpecialPowers.wrap(principal.URI).spec,
                                                 appId: principal.appId,
                                                 isInBrowserElement: true });

  iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;

  // Our test involves three <iframe mozbrowser>'s, parent, child1, and child2.
  // child1 and child2 are contained inside parent.  child1 is visibile, and
  // child2 is not.
  //
  // For the purposes of this test, we want there to be a process barrier
  // between child{1,2} and parent.  Therefore parent must be a non-remote
  // <iframe mozbrowser>, until bug 761935 is resolved and we can have nested
  // content processes.
  iframe.remote = false;

  iframe.addEventListener('mozbrowsershowmodalprompt', checkMessage);
  expectMessage('parent:ready', test1);

  document.body.appendChild(iframe);
  iframe.src = 'file_browserElement_SetVisibleFrames_Outer.html';
}

function test1() {
  expectMessage('child1:hidden', getVisibleTest1);
  iframe.setVisible(false);
}

function getVisibleTest1() {
  iframe.getVisible().onsuccess = function(evt) {
    ok(evt.target.result === false, 'getVisible shows a hidden frame');
    test2();
  };
}

function test2() {
  expectMessage('child1:visible', getVisibleTest2);
  iframe.setVisible(true);
}

function getVisibleTest2() {
  iframe.getVisible().onsuccess = function(evt) {
    ok(evt.target.result === true, 'getVisible shows a displayed frame');
    finish();
  };
}

function finish() {
  // We need to remove this listener because when this test finishes and the
  // iframe containing this document is navigated, we'll fire a
  // visibilitychange(false) event on all child iframes.  That's OK and
  // expected, but if we don't remove our listener, then we'll end up causing
  // the /next/ test to fail!
  iframe.removeEventListener('mozbrowsershowmodalprompt', checkMessage);

  var principal = SpecialPowers.wrap(SpecialPowers.getNodePrincipal(document));
  SpecialPowers.removePermission("browser", { url: SpecialPowers.wrap(principal.URI).spec,
                                              appId: principal.appId,
                                              isInBrowserElement: true });

  SimpleTest.finish();
}

var expectedMsg = null;
var expectedMsgCallback = null;
function expectMessage(msg, next) {
  expectedMsg = msg;
  expectedMsgCallback = next;
}

function checkMessage(e) {
  var msg = e.detail.message;
  is(msg, expectedMsg);
  if (msg == expectedMsg) {
    expectedMsg = null;
    SimpleTest.executeSoon(function() { expectedMsgCallback() });
  }
}

addEventListener('testready', runTest);
