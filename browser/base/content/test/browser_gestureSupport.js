/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Simple gestures tests
//
// These tests require the ability to disable the fact that the
// Firefox chrome intentionally prevents "simple gesture" events from
// reaching web content.

let test_utils;
let test_commandset;
let test_prefBranch = "browser.gesture.";

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

  // Reenable the default gestures support. The remaining tests target
  // the Firefox gesture functionality.
  gGestureSupport.init(true);

  // Test Firefox's gestures support.
  test_commandset = document.getElementById("mainCommandSet");
  test_swipeGestures();
  test_latchedGesture("pinch", "out", "in", "MozMagnifyGesture");
  test_latchedGesture("twist", "right", "left", "MozRotateGesture");
  test_thresholdGesture("pinch", "out", "in", "MozMagnifyGesture");
  test_thresholdGesture("twist", "right", "left", "MozRotateGesture");
}

let test_eventCount = 0;
let test_expectedType;
let test_expectedDirection;
let test_expectedDelta;
let test_expectedModifiers;
let test_expectedClickCount;

function test_gestureListener(evt)
{
  is(evt.type, test_expectedType,
     "evt.type (" + evt.type + ") does not match expected value");
  is(evt.target, test_utils.elementFromPoint(20, 20, false, false),
     "evt.target (" + evt.target + ") does not match expected value");
  is(evt.clientX, 20,
     "evt.clientX (" + evt.clientX + ") does not match expected value");
  is(evt.clientY, 20,
     "evt.clientY (" + evt.clientY + ") does not match expected value");
  isnot(evt.screenX, 0,
        "evt.screenX (" + evt.screenX + ") does not match expected value");
  isnot(evt.screenY, 0,
        "evt.screenY (" + evt.screenY + ") does not match expected value");

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

  if (evt.type == "MozTapGesture") {
    is(evt.clickCount, test_expectedClickCount, "evt.clickCount does not match");
  }

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
  test_utils.sendSimpleGestureEvent(type, 20, 20, direction, delta, modifiers);
  document.removeEventListener(type, test_gestureListener, true);

  is(expectedEventCount, test_eventCount, "Event (" + type + ") was never received by event listener");
}

function test_clicks(type, clicks)
{
  // Setup the expected values
  test_expectedType = type;
  test_expectedDirection = 0;
  test_expectedDelta = 0;
  test_expectedModifiers = 0;
  test_expectedClickCount = clicks;

  let expectedEventCount = test_eventCount + 1;

  document.addEventListener(type, test_gestureListener, true);
  test_utils.sendSimpleGestureEvent(type, 20, 20, 0, 0, 0, clicks);
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
  e("MozRotateGestureStart", SimpleGestureEvent.ROTATION_CLOCKWISE, 33.0, 0);
  e("MozRotateGestureUpdate", SimpleGestureEvent.ROTATION_COUNTERCLOCKWISE, -13.0, 0);
  e("MozRotateGestureUpdate", SimpleGestureEvent.ROTATION_CLOCKWISE, 13.0, 0);
  e("MozRotateGesture", SimpleGestureEvent.ROTATION_CLOCKWISE, 33.0, 0);
  
  // Tap and presstap gesture events
  test_clicks("MozTapGesture", 1);
  test_clicks("MozTapGesture", 2);
  test_clicks("MozTapGesture", 3);
  test_clicks("MozPressTapGesture", 1);

  // simple delivery test for edgeui gesture
  e("MozEdgeUIGesture", 0, 0, 0);

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
    event.initSimpleGestureEvent(type, true, true, window, 1,
                                 10, 10, 10, 10,
                                 ctrlKey, altKey, shiftKey, metaKey,
                                 1, window,
                                 direction, delta, 0);
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
  is(event.view, window, "Mismatch on evt.view");
  is(event.detail, 1, "Mismatch on evt.detail");
  is(event.clientX, 10, "Mismatch on evt.clientX");
  is(event.clientY, 10, "Mismatch on evt.clientY");
  is(event.screenX, 10, "Mismatch on evt.screenX");
  is(event.screenY, 10, "Mismatch on evt.screenY");
  is(event.button, 1, "Mismatch on evt.button");
  is(event.relatedTarget, window, "Mismatch on evt.relatedTarget");

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

  let clockwise = SimpleGestureEvent.ROTATION_CLOCKWISE;
  let cclockwise = SimpleGestureEvent.ROTATION_COUNTERCLOCKWISE;

  ok(up ^ down, "DIRECTION_UP and DIRECTION_DOWN are not bitwise disjoint");
  ok(up ^ left, "DIRECTION_UP and DIRECTION_LEFT are not bitwise disjoint");
  ok(up ^ right, "DIRECTION_UP and DIRECTION_RIGHT are not bitwise disjoint");
  ok(down ^ left, "DIRECTION_DOWN and DIRECTION_LEFT are not bitwise disjoint");
  ok(down ^ right, "DIRECTION_DOWN and DIRECTION_RIGHT are not bitwise disjoint");
  ok(left ^ right, "DIRECTION_LEFT and DIRECTION_RIGHT are not bitwise disjoint");
  ok(clockwise ^ cclockwise, "ROTATION_CLOCKWISE and ROTATION_COUNTERCLOCKWISE are not bitwise disjoint");
}

// Helper for test of latched event processing. Emits the actual
// gesture events to test whether the commands associated with the
// gesture will only trigger once for each direction of movement.
function test_emitLatchedEvents(eventPrefix, initialDelta, cmd)
{
  let cumulativeDelta = 0;
  let isIncreasing = initialDelta > 0;

  let expect = {};
  // Reset the call counters and initialize expected values
  for (let dir in cmd)
    cmd[dir].callCount = expect[dir] = 0;

  let check = function(aDir, aMsg) ok(cmd[aDir].callCount == expect[aDir], aMsg);
  let checkBoth = function(aNum, aInc, aDec) {
    let prefix = "Step " + aNum + ": ";
    check("inc", prefix + aInc);
    check("dec", prefix + aDec);
  };

  // Send the "Start" event.
  test_utils.sendSimpleGestureEvent(eventPrefix + "Start", 0, 0, 0, initialDelta, 0);
  cumulativeDelta += initialDelta;
  if (isIncreasing) {
    expect.inc++;
    checkBoth(1, "Increasing command was not triggered", "Decreasing command was triggered");
  } else {
    expect.dec++;
    checkBoth(1, "Increasing command was triggered", "Decreasing command was not triggered");
  }

  // Send random values in the same direction and ensure neither
  // command triggers.
  for (let i = 0; i < 5; i++) {
      let delta = Math.random() * (isIncreasing ? 100 : -100);
    test_utils.sendSimpleGestureEvent(eventPrefix + "Update", 0, 0, 0, delta, 0);
    cumulativeDelta += delta;
    checkBoth(2, "Increasing command was triggered", "Decreasing command was triggered");
  }

  // Now go back in the opposite direction.
  test_utils.sendSimpleGestureEvent(eventPrefix + "Update", 0, 0, 0,
				    - initialDelta, 0);
  cumulativeDelta += - initialDelta;
  if (isIncreasing) {
    expect.dec++;
    checkBoth(3, "Increasing command was triggered", "Decreasing command was not triggered");
  } else {
    expect.inc++;
    checkBoth(3, "Increasing command was not triggered", "Decreasing command was triggered");
  }

  // Send random values in the opposite direction and ensure neither
  // command triggers.
  for (let i = 0; i < 5; i++) {
    let delta = Math.random() * (isIncreasing ? -100 : 100);
    test_utils.sendSimpleGestureEvent(eventPrefix + "Update", 0, 0, 0, delta, 0);
    cumulativeDelta += delta;
    checkBoth(4, "Increasing command was triggered", "Decreasing command was triggered");
  }

  // Go back to the original direction. The original command should trigger.
  test_utils.sendSimpleGestureEvent(eventPrefix + "Update", 0, 0, 0,
				    initialDelta, 0);
  cumulativeDelta += initialDelta;
  if (isIncreasing) {
    expect.inc++;
    checkBoth(5, "Increasing command was not triggered", "Decreasing command was triggered");
  } else {
    expect.dec++;
    checkBoth(5, "Increasing command was triggered", "Decreasing command was not triggered");
  }

  // Send the wrap-up event. No commands should be triggered.
  test_utils.sendSimpleGestureEvent(eventPrefix, 0, 0, 0, cumulativeDelta, 0);
  checkBoth(6, "Increasing command was triggered", "Decreasing command was triggered");
}

function test_addCommand(prefName, id)
{
  let cmd = test_commandset.appendChild(document.createElement("command"));
  cmd.setAttribute("id", id);
  cmd.setAttribute("oncommand", "this.callCount++;");

  cmd.origPrefName = prefName;
  cmd.origPrefValue = gPrefService.getCharPref(prefName);
  gPrefService.setCharPref(prefName, id);

  return cmd;
}

function test_removeCommand(cmd)
{
  gPrefService.setCharPref(cmd.origPrefName, cmd.origPrefValue);
  test_commandset.removeChild(cmd);
}

// Test whether latched events are only called once per direction of motion.
function test_latchedGesture(gesture, inc, dec, eventPrefix)
{
  let branch = test_prefBranch + gesture + ".";

  // Put the gesture into latched mode.
  let oldLatchedValue = gPrefService.getBoolPref(branch + "latched");
  gPrefService.setBoolPref(branch + "latched", true);

  // Install the test commands for increasing and decreasing motion.
  let cmd = {
    inc: test_addCommand(branch + inc, "test:incMotion"),
    dec: test_addCommand(branch + dec, "test:decMotion"),
  };

  // Test the gestures in each direction.
  test_emitLatchedEvents(eventPrefix, 500, cmd);
  test_emitLatchedEvents(eventPrefix, -500, cmd);

  // Restore the gesture to its original configuration.
  gPrefService.setBoolPref(branch + "latched", oldLatchedValue);
  for (let dir in cmd)
    test_removeCommand(cmd[dir]);
}

// Test whether non-latched events are triggered upon sufficient motion.
function test_thresholdGesture(gesture, inc, dec, eventPrefix)
{
  let branch = test_prefBranch + gesture + ".";

  // Disable latched mode for this gesture.
  let oldLatchedValue = gPrefService.getBoolPref(branch + "latched");
  gPrefService.setBoolPref(branch + "latched", false);

  // Set the triggering threshold value to 50.
  let oldThresholdValue = gPrefService.getIntPref(branch + "threshold");
  gPrefService.setIntPref(branch + "threshold", 50);

  // Install the test commands for increasing and decreasing motion.
  let cmdInc = test_addCommand(branch + inc, "test:incMotion");
  let cmdDec = test_addCommand(branch + dec, "test:decMotion");

  // Send the start event but stop short of triggering threshold.
  cmdInc.callCount = cmdDec.callCount = 0;
  test_utils.sendSimpleGestureEvent(eventPrefix + "Start", 0, 0, 0, 49.5, 0);
  ok(cmdInc.callCount == 0, "Increasing command was triggered");
  ok(cmdDec.callCount == 0, "Decreasing command was triggered");

  // Now trigger the threshold.
  cmdInc.callCount = cmdDec.callCount = 0;
  test_utils.sendSimpleGestureEvent(eventPrefix + "Update", 0, 0, 0, 1, 0);
  ok(cmdInc.callCount == 1, "Increasing command was not triggered");
  ok(cmdDec.callCount == 0, "Decreasing command was triggered");

  // The tracking counter should go to zero. Go back the other way and
  // stop short of triggering the threshold.
  cmdInc.callCount = cmdDec.callCount = 0;
  test_utils.sendSimpleGestureEvent(eventPrefix + "Update", 0, 0, 0, -49.5, 0);
  ok(cmdInc.callCount == 0, "Increasing command was triggered");
  ok(cmdDec.callCount == 0, "Decreasing command was triggered");

  // Now cross the threshold and trigger the decreasing command.
  cmdInc.callCount = cmdDec.callCount = 0;
  test_utils.sendSimpleGestureEvent(eventPrefix + "Update", 0, 0, 0, -1.5, 0);
  ok(cmdInc.callCount == 0, "Increasing command was triggered");
  ok(cmdDec.callCount == 1, "Decreasing command was not triggered");

  // Send the wrap-up event. No commands should trigger.
  cmdInc.callCount = cmdDec.callCount = 0;
  test_utils.sendSimpleGestureEvent(eventPrefix, 0, 0, 0, -0.5, 0);
  ok(cmdInc.callCount == 0, "Increasing command was triggered");
  ok(cmdDec.callCount == 0, "Decreasing command was triggered");

  // Restore the gesture to its original configuration.
  gPrefService.setBoolPref(branch + "latched", oldLatchedValue);
  gPrefService.setIntPref(branch + "threshold", oldThresholdValue);
  test_removeCommand(cmdInc);
  test_removeCommand(cmdDec);
}

function test_swipeGestures()
{
  // easier to type names for the direction constants
  let up = SimpleGestureEvent.DIRECTION_UP;
  let down = SimpleGestureEvent.DIRECTION_DOWN;
  let left = SimpleGestureEvent.DIRECTION_LEFT;
  let right = SimpleGestureEvent.DIRECTION_RIGHT;

  let branch = test_prefBranch + "swipe.";

  // Install the test commands for the swipe gestures.
  let cmdUp = test_addCommand(branch + "up", "test:swipeUp");
  let cmdDown = test_addCommand(branch + "down", "test:swipeDown");
  let cmdLeft = test_addCommand(branch + "left", "test:swipeLeft");
  let cmdRight = test_addCommand(branch + "right", "test:swipeRight");

  function resetCounts() {
    cmdUp.callCount = 0;
    cmdDown.callCount = 0;
    cmdLeft.callCount = 0;
    cmdRight.callCount = 0;
  }

  // UP
  resetCounts();
  test_utils.sendSimpleGestureEvent("MozSwipeGesture", 0, 0, up, 0, 0);
  ok(cmdUp.callCount == 1, "Step 1: Up command was not triggered");
  ok(cmdDown.callCount == 0, "Step 1: Down command was triggered");
  ok(cmdLeft.callCount == 0, "Step 1: Left command was triggered");
  ok(cmdRight.callCount == 0, "Step 1: Right command was triggered");

  // DOWN
  resetCounts();
  test_utils.sendSimpleGestureEvent("MozSwipeGesture", 0, 0, down, 0, 0);
  ok(cmdUp.callCount == 0, "Step 2: Up command was triggered");
  ok(cmdDown.callCount == 1, "Step 2: Down command was not triggered");
  ok(cmdLeft.callCount == 0, "Step 2: Left command was triggered");
  ok(cmdRight.callCount == 0, "Step 2: Right command was triggered");

  // LEFT
  resetCounts();
  test_utils.sendSimpleGestureEvent("MozSwipeGesture", 0, 0, left, 0, 0);
  ok(cmdUp.callCount == 0, "Step 3: Up command was triggered");
  ok(cmdDown.callCount == 0, "Step 3: Down command was triggered");
  ok(cmdLeft.callCount == 1, "Step 3: Left command was not triggered");
  ok(cmdRight.callCount == 0, "Step 3: Right command was triggered");

  // RIGHT
  resetCounts();
  test_utils.sendSimpleGestureEvent("MozSwipeGesture", 0, 0, right, 0, 0);
  ok(cmdUp.callCount == 0, "Step 4: Up command was triggered");
  ok(cmdDown.callCount == 0, "Step 4: Down command was triggered");
  ok(cmdLeft.callCount == 0, "Step 4: Left command was triggered");
  ok(cmdRight.callCount == 1, "Step 4: Right command was not triggered");

  // Make sure combinations do not trigger events.
  let combos = [ up | left, up | right, down | left, down | right];
  for (let i = 0; i < combos.length; i++) {
    resetCounts();
    test_utils.sendSimpleGestureEvent("MozSwipeGesture", 0, 0, combos[i], 0, 0);
    ok(cmdUp.callCount == 0, "Step 5-"+i+": Up command was triggered");
    ok(cmdDown.callCount == 0, "Step 5-"+i+": Down command was triggered");
    ok(cmdLeft.callCount == 0, "Step 5-"+i+": Left command was triggered");
    ok(cmdRight.callCount == 0, "Step 5-"+i+": Right command was triggered");
  }

  // Remove the test commands.
  test_removeCommand(cmdUp);
  test_removeCommand(cmdDown);
  test_removeCommand(cmdLeft);
  test_removeCommand(cmdRight);
}
