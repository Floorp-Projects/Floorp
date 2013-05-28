/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the onmozbrowservisibilitychange event works.
'use strict';

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframe1 = null;
function runTest() {
  iframe1 = document.createElement('iframe');
  SpecialPowers.wrap(iframe1).mozbrowser = true;
  document.body.appendChild(iframe1);

  iframe1.src = 'data:text/html,<html><head><title>Title</title></head><body></body></html>';
  checkVisibilityFalse();
}

function checkVisibilityFalse() {
  iframe1.addEventListener('mozbrowservisibilitychange', function onvisibilitychange(e) {
    iframe1.removeEventListener(e.type, onvisibilitychange);

    is(e.detail.visible, false, 'Visibility should be false');
    checkVisibilityTrue();
  });

  iframe1.setVisible(false);
}

function checkVisibilityTrue() {
  iframe1.addEventListener('mozbrowservisibilitychange', function onvisibilitychange(e) {
    iframe1.removeEventListener(e.type, onvisibilitychange);

    is(e.detail.visible, true, 'Visibility should be true');
    SimpleTest.finish();
  });

  iframe1.setVisible(true);
}

addEventListener('testready', runTest);
