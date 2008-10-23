/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Firefox Gesture Support Test Code
 *
 * The Initial Developer of the Original Code is
 * Thomas K. Dyas <tdyas@zecador.org>
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Edward Lee <edward.lee@engineering.uiuc.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Simple gestures tests
//
// These tests require the ability to disable the fact that the
// Firefox chrome intentionally prevents "simple gesture" events from
// reaching web content.

let test_utils;

function test()
{
  // Disable the default gestures support during the test
  gGestureSupport.init(false);

  // Enable privileges so we can use nsIDOMWindowUtils interface
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  test_utils = window.QueryInterface(Components.interfaces.nsIInterfaceRequestor).
    getInterface(Components.interfaces.nsIDOMWindowUtils);

  // Run the tests of "simple gesture" events generally
  test_EnsureConstantsAreDisjoint();
  test_TestEventListeners();
  test_TestEventCreation();

  // Reenable the default gestures support
  gGestureSupport.init(true);
}

let test_eventCount = 0;
let test_expectedType;
let test_expectedDirection;
let test_expectedDelta;
let test_expectedModifiers;

function test_gestureListener(evt)
{
  is(evt.type, test_expectedType,
     "evt.type (" + evt.type + ") does not match expected value");
  is(evt.direction, test_expectedDirection,
     "evt.direction (" + evt.direction + ") does not match expected value");
  is(evt.delta, test_expectedDelta,
     "evt.delta (" + evt.delta + ") does not match expected value");

  is(evt.shiftKey, (test_expectedModifiers & Components.interfaces.nsIDOMNSEvent.SHIFT_MASK) != 0,
     "evt.shiftKey did not match expected value");
  is(evt.ctrlKey, (test_expectedModifiers & Components.interfaces.nsIDOMNSEvent.CONTROL_MASK) != 0,
     "evt.ctrlKey did not match expected value");
  is(evt.altKey, (test_expectedModifiers & Components.interfaces.nsIDOMNSEvent.ALT_MASK) != 0,
     "evt.altKey did not match expected value");
  is(evt.metaKey, (test_expectedModifiers & Components.interfaces.nsIDOMNSEvent.META_MASK) != 0,
     "evt.metaKey did not match expected value");

  test_eventCount++;
}

function test_helper1(type, direction, delta, modifiers)
{
  // Setup the expected values
  test_expectedType = type;
  test_expectedDirection = direction;
  test_expectedDelta = delta;
  test_expectedModifiers = modifiers;

  let expectedEventCount = test_eventCount + 1;

  document.addEventListener(type, test_gestureListener, true);
  test_utils.sendSimpleGestureEvent(type, direction, delta, modifiers);
  document.removeEventListener(type, test_gestureListener, true);

  is(expectedEventCount, test_eventCount, "Event (" + type + ") was never received by event listener");
}

function test_TestEventListeners()
{
  let e = test_helper1;  // easier to type this name

  // Swipe gesture event
  e("MozSwipeGesture", SimpleGestureEvent.DIRECTION_LEFT, 0.0, 0);
  e("MozSwipeGesture", SimpleGestureEvent.DIRECTION_RIGHT, 0.0, 0);
  e("MozSwipeGesture", SimpleGestureEvent.DIRECTION_UP, 0.0, 0);
  e("MozSwipeGesture", SimpleGestureEvent.DIRECTION_DOWN, 0.0, 0);
  e("MozSwipeGesture",
    SimpleGestureEvent.DIRECTION_UP | SimpleGestureEvent.DIRECTION_LEFT, 0.0, 0);
  e("MozSwipeGesture",
    SimpleGestureEvent.DIRECTION_DOWN | SimpleGestureEvent.DIRECTION_RIGHT, 0.0, 0);
  e("MozSwipeGesture",
    SimpleGestureEvent.DIRECTION_UP | SimpleGestureEvent.DIRECTION_RIGHT, 0.0, 0);
  e("MozSwipeGesture",
    SimpleGestureEvent.DIRECTION_DOWN | SimpleGestureEvent.DIRECTION_LEFT, 0.0, 0);

  // magnify gesture events
  e("MozMagnifyGestureStart", 0, 50.0, 0);
  e("MozMagnifyGestureUpdate", 0, -25.0, 0);
  e("MozMagnifyGestureUpdate", 0, 5.0, 0);
  e("MozMagnifyGesture", 0, 30.0, 0);

  // rotate gesture events
  e("MozRotateGestureStart", SimpleGestureEvent.DIRECTION_RIGHT, 33.0, 0);
  e("MozRotateGestureUpdate", SimpleGestureEvent.DIRECTION_LEFT, -13.0, 0);
  e("MozRotateGestureUpdate", SimpleGestureEvent.DIRECTION_RIGHT, 13.0, 0);
  e("MozRotateGesture", SimpleGestureEvent.DIRECTION_RIGHT, 33.0, 0);

  // event.shiftKey
  let modifier = Components.interfaces.nsIDOMNSEvent.SHIFT_MASK;
  e("MozSwipeGesture", SimpleGestureEvent.DIRECTION_RIGHT, 0, modifier);

  // event.metaKey
  modifier = Components.interfaces.nsIDOMNSEvent.META_MASK;
  e("MozSwipeGesture", SimpleGestureEvent.DIRECTION_RIGHT, 0, modifier);

  // event.altKey
  modifier = Components.interfaces.nsIDOMNSEvent.ALT_MASK;
  e("MozSwipeGesture", SimpleGestureEvent.DIRECTION_RIGHT, 0, modifier);

  // event.ctrlKey
  modifier = Components.interfaces.nsIDOMNSEvent.CONTROL_MASK;
  e("MozSwipeGesture", SimpleGestureEvent.DIRECTION_RIGHT, 0, modifier);
}

function test_eventDispatchListener(evt)
{
  test_eventCount++;
  evt.stopPropagation();
}

function test_helper2(type, direction, delta, altKey, ctrlKey, shiftKey, metaKey)
{
  let event = null;
  let successful;

  try {
    event = document.createEvent("SimpleGestureEvent");
    successful = true;
  }
  catch (ex) {
    successful = false;
  }
  ok(successful, "Unable to create SimpleGestureEvent");

  try {
    event.initSimpleGestureEvent(type, true, true, null, 0, direction,
                                 delta, altKey, ctrlKey, shiftKey, metaKey);
    successful = true;
  }
  catch (ex) {
    successful = false;
  }
  ok(successful, "event.initSimpleGestureEvent should not fail");

  // Make sure the event fields match the expected values
  is(event.type, type, "Mismatch on evt.type");
  is(event.direction, direction, "Mismatch on evt.direction");
  is(event.delta, delta, "Mismatch on evt.delta");
  is(event.altKey, altKey, "Mismatch on evt.altKey");
  is(event.ctrlKey, ctrlKey, "Mismatch on evt.ctrlKey");
  is(event.shiftKey, shiftKey, "Mismatch on evt.shiftKey");
  is(event.metaKey, metaKey, "Mismatch on evt.metaKey");

  // Test event dispatch
  let expectedEventCount = test_eventCount + 1;
  document.addEventListener(type, test_eventDispatchListener, true);
  document.dispatchEvent(event);
  document.removeEventListener(type, test_eventDispatchListener, true);
  is(expectedEventCount, test_eventCount, "Dispatched event was never received by listener");
}

function test_TestEventCreation()
{
  // Event creation
  test_helper2("MozMagnifyGesture", SimpleGestureEvent.DIRECTION_RIGHT, 20.0,
               true, false, true, false);
  test_helper2("MozMagnifyGesture", SimpleGestureEvent.DIRECTION_LEFT, -20.0,
               false, true, false, true);
}

function test_EnsureConstantsAreDisjoint()
{
  let up = SimpleGestureEvent.DIRECTION_UP;
  let down = SimpleGestureEvent.DIRECTION_DOWN;
  let left = SimpleGestureEvent.DIRECTION_LEFT;
  let right = SimpleGestureEvent.DIRECTION_RIGHT;

  ok(up ^ down, "DIRECTION_UP and DIRECTION_DOWN are not bitwise disjoint");
  ok(up ^ left, "DIRECTION_UP and DIRECTION_LEFT are not bitwise disjoint");
  ok(up ^ right, "DIRECTION_UP and DIRECTION_RIGHT are not bitwise disjoint");
  ok(down ^ left, "DIRECTION_DOWN and DIRECTION_LEFT are not bitwise disjoint");
  ok(down ^ right, "DIRECTION_DOWN and DIRECTION_RIGHT are not bitwise disjoint");
  ok(left ^ right, "DIRECTION_LEFT and DIRECTION_RIGHT are not bitwise disjoint");
}
