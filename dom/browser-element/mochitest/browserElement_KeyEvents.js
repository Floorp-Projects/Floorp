/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that an iframe with the |mozbrowser| attribute does bubble some
// whitelisted key events.
"use strict";

let Ci = Components.interfaces;

let whitelistedEvents = [
  Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE,   // Back button.
  Ci.nsIDOMKeyEvent.DOM_VK_SLEEP,    // Power button
  Ci.nsIDOMKeyEvent.DOM_VK_CONTEXT_MENU,
  Ci.nsIDOMKeyEvent.DOM_VK_F5,       // Search button.
  Ci.nsIDOMKeyEvent.DOM_VK_PAGE_UP,  // Volume up.
  Ci.nsIDOMKeyEvent.DOM_VK_PAGE_DOWN // Volume down.
];

SimpleTest.waitForExplicitFinish();

browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addToWhitelist();
browserElementTestHelpers.setOOPDisabledPref(true); // this is breaking the autofocus.

var iframe = document.createElement('iframe');
iframe.mozbrowser = true;
iframe.src = browserElementTestHelpers.focusPage;
document.body.appendChild(iframe);

// Number of expected events at which point we will consider the test as done.
var nbEvents = whitelistedEvents.length * 3;

function eventHandler(e) {
  ok(((e.type == 'keydown' || e.type == 'keypress' || e.type == 'keyup') &&
      !e.defaultPrevented &&
      (whitelistedEvents.indexOf(e.keyCode) != -1)),
      "[ " + e.type + "] Handled event should be a non prevented key event in the white list.");

  nbEvents--;

  if (nbEvents == 0) {
    browserElementTestHelpers.restoreOriginalPrefs();
    SimpleTest.finish();
    return;
  }

  if (nbEvents < 0) {
    ok(false, "got an unexpected event! " + e.type + " " + e.keyCode);
    SimpleTest.finish();
    return;
  }
}

function runTest() {
  is(document.activeElement, iframe, "iframe should be focused");

  addEventListener('keydown', eventHandler);
  addEventListener('keypress', eventHandler);
  addEventListener('keyup', eventHandler);

  // Those event should not be received because not whitelisted.
  synthesizeKey("VK_A", {});
  synthesizeKey("VK_B", {});

  // Those events should not be received because prevent default is called.
  synthesizeKey("VK_ESCAPE", {});

  // Those events should be received.
  synthesizeKey("VK_F5", {}); // F5 key is going to be canceled by ESC key.
  synthesizeKey("VK_ESCAPE", {});
  synthesizeKey("VK_PAGE_UP", {});   // keypress is ignored because .preventDefault() will be called.
  synthesizeKey("VK_PAGE_DOWN", {}); // keypress is ignored because .preventDefault() will be called.
  synthesizeKey("VK_CONTEXT_MENU", {});
  synthesizeKey("VK_SLEEP", {});
}

SimpleTest.waitForFocus(function() {
  iframe.focus();
  SimpleTest.executeSoon(runTest);
});

