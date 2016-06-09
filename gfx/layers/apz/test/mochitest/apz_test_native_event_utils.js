// Utilities for synthesizing of native events.

function getPlatform() {
  if (navigator.platform.indexOf("Win") == 0) {
    return "windows";
  }
  if (navigator.platform.indexOf("Mac") == 0) {
    return "mac";
  }
  // Check for Android before Linux
  if (navigator.appVersion.indexOf("Android") >= 0) {
    return "android"
  }
  if (navigator.platform.indexOf("Linux") == 0) {
    return "linux";
  }
  return "unknown";
}

function nativeVerticalWheelEventMsg() {
  switch (getPlatform()) {
    case "windows": return 0x020A; // WM_MOUSEWHEEL
    case "mac": return 0; // value is unused, can be anything
    case "linux": return 4; // value is unused, pass GDK_SCROLL_SMOOTH anyway
  }
  throw "Native wheel events not supported on platform " + getPlatform();
}

function nativeHorizontalWheelEventMsg() {
  switch (getPlatform()) {
    case "windows": return 0x020E; // WM_MOUSEHWHEEL
    case "mac": return 0; // value is unused, can be anything
    case "linux": return 4; // value is unused, pass GDK_SCROLL_SMOOTH anyway
  }
  throw "Native wheel events not supported on platform " + getPlatform();
}

// Given a pixel scrolling delta, converts it to the platform's native units.
function nativeScrollUnits(aElement, aDimen) {
  switch (getPlatform()) {
    case "linux": {
      // GTK deltas are treated as line height divided by 3 by gecko.
      var targetWindow = aElement.ownerDocument.defaultView;
      var lineHeight = targetWindow.getComputedStyle(aElement)["font-size"];
      return aDimen / (parseInt(lineHeight) * 3);
    }
  }
  return aDimen;
}

function nativeMouseDownEventMsg() {
  switch (getPlatform()) {
    case "windows": return 2; // MOUSEEVENTF_LEFTDOWN
    case "mac": return 1; // NSLeftMouseDown
    case "linux": return 4; // GDK_BUTTON_PRESS
    case "android": return 5; // ACTION_POINTER_DOWN
  }
  throw "Native mouse-down events not supported on platform " + getPlatform();
}

function nativeMouseMoveEventMsg() {
  switch (getPlatform()) {
    case "windows": return 1; // MOUSEEVENTF_MOVE
    case "mac": return 5; // NSMouseMoved
    case "linux": return 3; // GDK_MOTION_NOTIFY
    case "android": return 7; // ACTION_HOVER_MOVE
  }
  throw "Native mouse-move events not supported on platform " + getPlatform();
}

function nativeMouseUpEventMsg() {
  switch (getPlatform()) {
    case "windows": return 4; // MOUSEEVENTF_LEFTUP
    case "mac": return 2; // NSLeftMouseUp
    case "linux": return 7; // GDK_BUTTON_RELEASE
    case "android": return 6; // ACTION_POINTER_UP
  }
  throw "Native mouse-up events not supported on platform " + getPlatform();
}

// Convert (aX, aY), in CSS pixels relative to aElement's bounding rect,
// to device pixels relative to the screen.
function coordinatesRelativeToScreen(aX, aY, aElement) {
  var targetWindow = aElement.ownerDocument.defaultView;
  var scale = targetWindow.devicePixelRatio;
  var rect = aElement.getBoundingClientRect();
  return {
    x: (targetWindow.mozInnerScreenX + rect.left + aX) * scale,
    y: (targetWindow.mozInnerScreenY + rect.top + aY) * scale
  };
}

// Get the bounding box of aElement, and return it in device pixels
// relative to the screen.
function rectRelativeToScreen(aElement) {
  var targetWindow = aElement.ownerDocument.defaultView;
  var scale = targetWindow.devicePixelRatio;
  var rect = aElement.getBoundingClientRect();
  return {
    x: (targetWindow.mozInnerScreenX + rect.left) * scale,
    y: (targetWindow.mozInnerScreenY + rect.top) * scale,
    w: (rect.width * scale),
    h: (rect.height * scale)
  };
}

// Synthesizes a native mousewheel event and returns immediately. This does not
// guarantee anything; you probably want to use one of the other functions below
// which actually wait for results.
// aX and aY are relative to the top-left of |aElement|'s containing window.
// aDeltaX and aDeltaY are pixel deltas, and aObserver can be left undefined
// if not needed.
function synthesizeNativeWheel(aElement, aX, aY, aDeltaX, aDeltaY, aObserver) {
  var pt = coordinatesRelativeToScreen(aX, aY, aElement);
  if (aDeltaX && aDeltaY) {
    throw "Simultaneous wheeling of horizontal and vertical is not supported on all platforms.";
  }
  aDeltaX = nativeScrollUnits(aElement, aDeltaX);
  aDeltaY = nativeScrollUnits(aElement, aDeltaY);
  var msg = aDeltaX ? nativeHorizontalWheelEventMsg() : nativeVerticalWheelEventMsg();
  var utils = SpecialPowers.getDOMWindowUtils(aElement.ownerDocument.defaultView);
  utils.sendNativeMouseScrollEvent(pt.x, pt.y, msg, aDeltaX, aDeltaY, 0, 0, 0, aElement, aObserver);
  return true;
}

// Synthesizes a native mousewheel event and invokes the callback once the
// request has been successfully made to the OS. This does not necessarily
// guarantee that the OS generates the event we requested. See
// synthesizeNativeWheel for details on the parameters.
function synthesizeNativeWheelAndWaitForObserver(aElement, aX, aY, aDeltaX, aDeltaY, aCallback) {
  var observer = {
    observe: function(aSubject, aTopic, aData) {
      if (aCallback && aTopic == "mousescrollevent") {
        setTimeout(aCallback, 0);
      }
    }
  };
  return synthesizeNativeWheel(aElement, aX, aY, aDeltaX, aDeltaY, observer);
}

// Synthesizes a native mousewheel event and invokes the callback once the
// wheel event is dispatched to |aElement|'s containing window. If the event
// targets content in a subdocument, |aElement| should be inside the
// subdocument. See synthesizeNativeWheel for details on the other parameters.
function synthesizeNativeWheelAndWaitForWheelEvent(aElement, aX, aY, aDeltaX, aDeltaY, aCallback) {
  var targetWindow = aElement.ownerDocument.defaultView;
  targetWindow.addEventListener("wheel", function wheelWaiter(e) {
    targetWindow.removeEventListener("wheel", wheelWaiter);
    setTimeout(aCallback, 0);
  });
  return synthesizeNativeWheel(aElement, aX, aY, aDeltaX, aDeltaY);
}

// Synthesizes a native mousewheel event and invokes the callback once the
// first resulting scroll event is dispatched to |aElement|'s containing window.
// If the event targets content in a subdocument, |aElement| should be inside
// the subdocument.  See synthesizeNativeWheel for details on the other
// parameters.
function synthesizeNativeWheelAndWaitForScrollEvent(aElement, aX, aY, aDeltaX, aDeltaY, aCallback) {
  var targetWindow = aElement.ownerDocument.defaultView;
  var useCapture = true;  // scroll events don't always bubble
  targetWindow.addEventListener("scroll", function scrollWaiter(e) {
    targetWindow.removeEventListener("scroll", scrollWaiter, useCapture);
    setTimeout(aCallback, 0);
  }, useCapture);
  return synthesizeNativeWheel(aElement, aX, aY, aDeltaX, aDeltaY);
}

// Synthesizes a native mouse move event and returns immediately.
// aX and aY are relative to the top-left of |aElement|'s containing window.
function synthesizeNativeMouseMove(aElement, aX, aY) {
  var pt = coordinatesRelativeToScreen(aX, aY, aElement);
  var utils = SpecialPowers.getDOMWindowUtils(aElement.ownerDocument.defaultView);
  utils.sendNativeMouseEvent(pt.x, pt.y, nativeMouseMoveEventMsg(), 0, aElement);
  return true;
}

// Synthesizes a native mouse move event and invokes the callback once the
// mouse move event is dispatched to |aElement|'s containing window. If the event
// targets content in a subdocument, |aElement| should be inside the
// subdocument. See synthesizeNativeMouseMove for details on the other
// parameters.
function synthesizeNativeMouseMoveAndWaitForMoveEvent(aElement, aX, aY, aCallback) {
  var targetWindow = aElement.ownerDocument.defaultView;
  targetWindow.addEventListener("mousemove", function mousemoveWaiter(e) {
    targetWindow.removeEventListener("mousemove", mousemoveWaiter);
    setTimeout(aCallback, 0);
  });
  return synthesizeNativeMouseMove(aElement, aX, aY);
}

// Synthesizes a native touch event and dispatches it. aX and aY in CSS pixels
// relative to the top-left of |aElement|'s bounding rect.
function synthesizeNativeTouch(aElement, aX, aY, aType, aObserver = null, aTouchId = 0) {
  var pt = coordinatesRelativeToScreen(aX, aY, aElement);
  var utils = SpecialPowers.getDOMWindowUtils(aElement.ownerDocument.defaultView);
  utils.sendNativeTouchPoint(aTouchId, aType, pt.x, pt.y, 1, 90, aObserver);
  return true;
}

// A handy constant when synthesizing native touch drag events with the pref
// "apz.touch_start_tolerance" set to 0. In this case, the first touchmove with
// a nonzero pixel movement is consumed by the APZ to transition from the
// "touching" state to the "panning" state, so calls to synthesizeNativeTouchDrag
// should add an extra pixel pixel for this purpose. The TOUCH_SLOP provides
// a constant that can be used for this purpose. Note that if the touch start
// tolerance is set to something higher, the touch slop amount used must be
// correspondingly increased so as to be higher than the tolerance.
const TOUCH_SLOP = 1;
function synthesizeNativeTouchDrag(aElement, aX, aY, aDeltaX, aDeltaY, aObserver = null, aTouchId = 0) {
  synthesizeNativeTouch(aElement, aX, aY, SpecialPowers.DOMWindowUtils.TOUCH_CONTACT, null, aTouchId);
  var steps = Math.max(Math.abs(aDeltaX), Math.abs(aDeltaY));
  for (var i = 1; i < steps; i++) {
    var dx = i * (aDeltaX / steps);
    var dy = i * (aDeltaY / steps);
    synthesizeNativeTouch(aElement, aX + dx, aY + dy, SpecialPowers.DOMWindowUtils.TOUCH_CONTACT, null, aTouchId);
  }
  synthesizeNativeTouch(aElement, aX + aDeltaX, aY + aDeltaY, SpecialPowers.DOMWindowUtils.TOUCH_CONTACT, null, aTouchId);
  return synthesizeNativeTouch(aElement, aX + aDeltaX, aY + aDeltaY, SpecialPowers.DOMWindowUtils.TOUCH_REMOVE, aObserver, aTouchId);
}

function synthesizeNativeTap(aElement, aX, aY, aObserver = null) {
  var pt = coordinatesRelativeToScreen(aX, aY, aElement);
  var utils = SpecialPowers.getDOMWindowUtils(aElement.ownerDocument.defaultView);
  utils.sendNativeTouchTap(pt.x, pt.y, false, aObserver);
  return true;
}

function synthesizeNativeMouseEvent(aElement, aX, aY, aType, aObserver = null) {
  var pt = coordinatesRelativeToScreen(aX, aY, aElement);
  var utils = SpecialPowers.getDOMWindowUtils(aElement.ownerDocument.defaultView);
  utils.sendNativeMouseEvent(pt.x, pt.y, aType, 0, aElement, aObserver);
  return true;
}

function synthesizeNativeClick(aElement, aX, aY, aObserver = null) {
  var pt = coordinatesRelativeToScreen(aX, aY, aElement);
  var utils = SpecialPowers.getDOMWindowUtils(aElement.ownerDocument.defaultView);
  utils.sendNativeMouseEvent(pt.x, pt.y, nativeMouseDownEventMsg(), 0, aElement, function() {
    utils.sendNativeMouseEvent(pt.x, pt.y, nativeMouseUpEventMsg(), 0, aElement, aObserver);
  });
  return true;
}

// Move the mouse to (dx, dy) relative to |element|, and scroll the wheel
// at that location.
// Moving the mouse is necessary to avoid wheel events from two consecutive
// moveMouseAndScrollWheelOver() calls on different elements being incorrectly
// considered as part of the same wheel transaction.
// We also wait for the mouse move event to be processed before sending the
// wheel event, otherwise there is a chance they might get reordered, and
// we have the transaction problem again.
function moveMouseAndScrollWheelOver(element, dx, dy, testDriver) {
  return synthesizeNativeMouseMoveAndWaitForMoveEvent(element, dx, dy, function() {
    synthesizeNativeWheelAndWaitForScrollEvent(element, dx, dy, 0, -10, testDriver);
  });
}
