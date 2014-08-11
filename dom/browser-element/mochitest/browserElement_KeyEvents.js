/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that an iframe with the |mozbrowser| attribute does bubble some
// whitelisted key events.
"use strict";

let Ci = SpecialPowers.Ci;

let whitelistedKeyCodes = [
  Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE,   // Back button.
  Ci.nsIDOMKeyEvent.DOM_VK_SLEEP,    // Power button
  Ci.nsIDOMKeyEvent.DOM_VK_CONTEXT_MENU,
  Ci.nsIDOMKeyEvent.DOM_VK_F5,       // Search button.
  Ci.nsIDOMKeyEvent.DOM_VK_PAGE_UP,  // Volume up.
  Ci.nsIDOMKeyEvent.DOM_VK_PAGE_DOWN // Volume down.
];

let blacklistedKeyCodes = [
  Ci.nsIDOMKeyEvent.DOM_VK_A,
  Ci.nsIDOMKeyEvent.DOM_VK_B
];

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

// Number of expected events at which point we will consider the test as done.
var nbEvents = whitelistedKeyCodes.length * 3;

var iframe;
var finished = false;
function runTest() {
  iframe = document.createElement('iframe');
  SpecialPowers.wrap(iframe).mozbrowser = true;
  iframe.src = browserElementTestHelpers.focusPage;

  var gotFocus = false;
  var gotLoadend = false;

  function maybeTest2() {
    if (gotFocus && gotLoadend) {
      SimpleTest.executeSoon(test2);
    }
  }

  iframe.addEventListener('mozbrowserloadend', function() {
    gotLoadend = true;
    maybeTest2();
  });

  document.body.appendChild(iframe);

  SimpleTest.waitForFocus(function() {
    iframe.focus();
    gotFocus = true;
    maybeTest2();
  });
}

function eventHandler(e) {
  if (whitelistedKeyCodes.indexOf(e.keyCode) == -1 &&
      blacklistedKeyCodes.indexOf(e.keyCode) == -1) {
    // See bug 856006: We sometimes get unexpected key presses, and we don't
    // know why.  Don't turn the test orange over this.
    ok(true, "Ignoring unexpected " + e.type +
       " with keyCode " + e.keyCode + ".");
    return;
  }

  ok(e.type == 'keydown' || e.type == 'keypress' || e.type == 'keyup',
     "e.type was " + e.type + ", expected keydown, keypress, or keyup");
  ok(!e.defaultPrevented, "expected !e.defaultPrevented");
  ok(whitelistedKeyCodes.indexOf(e.keyCode) != -1,
     "Expected a whitelited keycode, but got " + e.keyCode + " instead.");

  nbEvents--;

  // Prevent default for F5 because on desktop that reloads the page.
  if (e.keyCode === Ci.nsIDOMKeyEvent.DOM_VK_F5) {
    e.preventDefault();
  }

  if (nbEvents == 0) {
    SimpleTest.finish();
    return;
  }

  if (nbEvents < 0 && !finished) {
    ok(false, "got an unexpected event! " + e.type + " " + e.keyCode);
  }
}

function test2() {
  is(document.activeElement, iframe, "iframe should be focused");

  addEventListener('keydown', eventHandler);
  addEventListener('keypress', eventHandler);
  addEventListener('keyup', eventHandler);

  // These events should not be received because they are not whitelisted.
  synthesizeKey("VK_A", {});
  synthesizeKey("VK_B", {});

  // These events should not be received because preventDefault is called.
  synthesizeKey("VK_ESCAPE", {});

  // These events should be received.
  synthesizeKey("VK_F5", {});
  synthesizeKey("VK_ESCAPE", {});
  synthesizeKey("VK_PAGE_UP", {});
  synthesizeKey("VK_PAGE_DOWN", {});
  synthesizeKey("VK_CONTEXT_MENU", {});
  synthesizeKey("VK_SLEEP", {});
  finished = true;
}

addEventListener('testready', runTest);
