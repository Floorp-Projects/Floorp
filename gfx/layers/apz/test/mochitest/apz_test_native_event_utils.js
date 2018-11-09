// Utilities for synthesizing of native events.

function getResolution() {
  let resolution = { value: -1 }; // bogus value in case DWU fails us
  SpecialPowers.getDOMWindowUtils(window).getResolution(resolution);
  return resolution.value;
}

function getPlatform() {
  if (navigator.platform.indexOf("Win") == 0) {
    return "windows";
  }
  if (navigator.platform.indexOf("Mac") == 0) {
    return "mac";
  }
  // Check for Android before Linux
  if (navigator.appVersion.includes("Android")) {
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

// Given an event target which may be a window or an element, get the associated window.
function windowForTarget(aTarget) {
  if (aTarget instanceof Window) {
    return aTarget;
  }
  return aTarget.ownerDocument.defaultView;
}

function getBoundingClientRectRelativeToVisualViewport(aElement) {
  let utils = SpecialPowers.getDOMWindowUtils(window);
  var rect = aElement.getBoundingClientRect();
  var offsetX = {}, offsetY = {};
  utils.getVisualViewportOffsetRelativeToLayoutViewport(offsetX, offsetY);
  rect.x -= offsetX.value;
  rect.y -= offsetY.value;
  return rect;
}

// Several event sythesization functions below (and their helpers) take a "target" 
// parameter which may be either an element or a window. For such functions,
// the target's "bounding rect" refers to the bounding client rect for an element,
// and the window's origin for a window.
// Not all functions have been "upgraded" to allow a window argument yet; feel
// free to upgrade others as necessary.

// Convert (aX, aY), in CSS pixels relative to aTarget's bounding rect
// to device pixels relative to the screen.
function coordinatesRelativeToScreen(aX, aY, aTarget) {
  var targetWindow = windowForTarget(aTarget);
  var deviceScale = targetWindow.devicePixelRatio;
  var resolution = getResolution();
  var rect = (aTarget instanceof Window)
    ? {left: 0, top: 0} /* we don't use the width or height */
    : getBoundingClientRectRelativeToVisualViewport(aTarget);
  // moxInnerScreen{X,Y} are in CSS coordinates of the browser chrome.
  // The device scale applies to them, but the resolution only zooms the content.
  return {
    x: (targetWindow.mozInnerScreenX + ((rect.left + aX) * resolution)) * deviceScale,
    y: (targetWindow.mozInnerScreenY + ((rect.top + aY) * resolution)) * deviceScale
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
  targetWindow.addEventListener("wheel", function(e) {
    setTimeout(aCallback, 0);
  }, {once: true});
  return synthesizeNativeWheel(aElement, aX, aY, aDeltaX, aDeltaY);
}

// Synthesizes a native mousewheel event and invokes the callback once the
// first resulting scroll event is dispatched to |aElement|'s containing window.
// If the event targets content in a subdocument, |aElement| should be inside
// the subdocument.  See synthesizeNativeWheel for details on the other
// parameters.
function synthesizeNativeWheelAndWaitForScrollEvent(aElement, aX, aY, aDeltaX, aDeltaY, aCallback) {
  var targetWindow = aElement.ownerDocument.defaultView;
  targetWindow.addEventListener("scroll", function() {
    setTimeout(aCallback, 0);
  }, {capture: true, once: true}); // scroll events don't always bubble
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
  targetWindow.addEventListener("mousemove", function(e) {
    setTimeout(aCallback, 0);
  }, {once: true});
  return synthesizeNativeMouseMove(aElement, aX, aY);
}

// Synthesizes a native touch event and dispatches it. aX and aY in CSS pixels
// relative to the top-left of |aTarget|'s bounding rect.
function synthesizeNativeTouch(aTarget, aX, aY, aType, aObserver = null, aTouchId = 0) {
  var pt = coordinatesRelativeToScreen(aX, aY, aTarget);
  var utils = SpecialPowers.getDOMWindowUtils(windowForTarget(aTarget));
  utils.sendNativeTouchPoint(aTouchId, aType, pt.x, pt.y, 1, 90, aObserver);
  return true;
}

// Function to generate native touch events for a multi-touch sequence.
// aTarget is the element or window whose bounding rect the coordinates are relative to.
// aPositions is a 2D array of position data. It is indexed as [row][column],
//   where advancing the row counter moves forward in time, and each column
//   represents a single "finger" (or touch input). Each row must have exactly
//   the same number of columns, and the number of columns must match the length
//   of the aTouchIds parameter. However, rows are allowed to be null, this
//   represents a yield point, where the function yields back to the caller for
//   additional processing at that point in the touch sequence.
//   For each non-null row, each entry is either an object with x and y fields,
//   or a null. A null value indicates that the "finger" should be "lifted"
//   (i.e. send a touchend for that touch input). A non-null value therefore
//   indicates the position of the touch input.
//   This function takes care of the state tracking necessary to send
//   touchstart/touchend inputs as necessary as the fingers go up and down.
// aObserver is the observer that will get registered on the very last
//   synthesizeNativeTouch call this function makes.
// aTouchIds is an array holding the touch ID values of each "finger".
function* synthesizeNativeTouchSequences(aTarget, aPositions, aObserver = null, aTouchIds = [0]) {
  // We use lastNonNullValue to figure out which synthesizeNativeTouch call
  // will be the last one we make, so that we can register aObserver on it.
  var lastNonNullValue = -1;
  var yields = 0;
  for (var i = 0; i < aPositions.length; i++) {
    if (aPositions[i] == null) {
      yields++;
      continue;
    }
    if (aPositions[i].length != aTouchIds.length) {
      throw "aPositions[" + i + "] did not have the expected number of positions; expected " + aTouchIds.length + " touch points but found " + aPositions[i].length;
    }
    for (var j = 0; j < aTouchIds.length; j++) {
      if (aPositions[i][j] != null) {
        lastNonNullValue = ((i - yields) * aTouchIds.length) + j;
      }
    }
  }
  if (lastNonNullValue < 0) {
    throw "All values in positions array were null!";
  }

  // Insert a row of nulls at the end of aPositions, to ensure that all
  // touches get removed. If the touches have already been removed this will
  // just add an extra no-op iteration in the aPositions loop below.
  var allNullRow = new Array(aTouchIds.length);
  allNullRow.fill(null);
  aPositions.push(allNullRow);

  // The last synthesizeNativeTouch call will be the TOUCH_REMOVE which happens
  // one iteration of aPosition after the last non-null value.
  var lastSynthesizeCall = lastNonNullValue + aTouchIds.length;

  // track which touches are down and which are up. start with all up
  var currentPositions = new Array(aTouchIds.length);
  currentPositions.fill(null);

  // Iterate over the position data now, and generate the touches requested
  yields = 0;
  for (var i = 0; i < aPositions.length; i++) {
    if (aPositions[i] == null) {
      yields++;
      yield i;
      continue;
    }
    for (var j = 0; j < aTouchIds.length; j++) {
      if (aPositions[i][j] == null) {
        // null means lift the finger
        if (currentPositions[j] == null) {
          // it's already lifted, do nothing
        } else {
          // synthesize the touch-up. If this is the last call we're going to
          // make, pass the observer as well
          var thisIndex = ((i - yields) * aTouchIds.length) + j;
          var observer = (lastSynthesizeCall == thisIndex) ? aObserver : null;
          synthesizeNativeTouch(aTarget, currentPositions[j].x, currentPositions[j].y, SpecialPowers.DOMWindowUtils.TOUCH_REMOVE, observer, aTouchIds[j]);
          currentPositions[j] = null;
        }
      } else {
        synthesizeNativeTouch(aTarget, aPositions[i][j].x, aPositions[i][j].y, SpecialPowers.DOMWindowUtils.TOUCH_CONTACT, null, aTouchIds[j]);
        currentPositions[j] = aPositions[i][j];
      }
    }
  }
  return true;
}

// Note that when calling this function you'll want to make sure that the pref
// "apz.touch_start_tolerance" is set to 0, or some of the touchmove will get
// consumed to overcome the panning threshold.
function synthesizeNativeTouchDrag(aTarget, aX, aY, aDeltaX, aDeltaY, aObserver = null, aTouchId = 0) {
  var steps = Math.max(Math.abs(aDeltaX), Math.abs(aDeltaY));
  var positions = new Array();
  positions.push([{ x: aX, y: aY }]);
  for (var i = 1; i < steps; i++) {
    var dx = i * (aDeltaX / steps);
    var dy = i * (aDeltaY / steps);
    var pos = { x: aX + dx, y: aY + dy };
    positions.push([pos]);
  }
  positions.push([{ x: aX + aDeltaX, y: aY + aDeltaY }]);
  var continuation = synthesizeNativeTouchSequences(aTarget, positions, aObserver, [aTouchId]);
  var yielded = continuation.next();
  while (!yielded.done) {
    yielded = continuation.next();
  }
  return yielded.value;
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
function moveMouseAndScrollWheelOver(element, dx, dy, testDriver, waitForScroll = true) {
  return synthesizeNativeMouseMoveAndWaitForMoveEvent(element, dx, dy, function() {
    if (waitForScroll) {
      synthesizeNativeWheelAndWaitForScrollEvent(element, dx, dy, 0, -10, testDriver);
    } else {
      synthesizeNativeWheelAndWaitForWheelEvent(element, dx, dy, 0, -10, testDriver);
    }
  });
}

// Synthesizes events to drag |element|'s vertical scrollbar by the distance
// specified, synthesizing a mousemove for each increment as specified.
// Returns false if the element doesn't have a vertical scrollbar. Otherwise,
// returns a generator that should be invoked after the mousemoves have been
// processed by the widget code, to end the scrollbar drag. Mousemoves being
// processed by the widget code can be detected by listening for the mousemove
// events in the caller, or for some other event that is triggered by the
// mousemove, such as the scroll event resulting from the scrollbar drag.
// Note: helper_scrollbar_snap_bug1501062.html contains a copy of this code
// with modifications. Fixes here should be copied there if appropriate.
function* dragVerticalScrollbar(element, testDriver, distance = 20, increment = 5) {
  var boundingClientRect = element.getBoundingClientRect();
  var verticalScrollbarWidth = boundingClientRect.width - element.clientWidth;
  if (verticalScrollbarWidth == 0) {
    return false;
  }

  var upArrowHeight = verticalScrollbarWidth; // assume square scrollbar buttons
  var mouseX = element.clientWidth + (verticalScrollbarWidth / 2);
  var mouseY = upArrowHeight + 5; // start dragging somewhere in the thumb

  dump("Starting drag at " + mouseX + ", " + mouseY + " from top-left of #" + element.id + "\n");

  // Move the mouse to the scrollbar thumb and drag it down
  yield synthesizeNativeMouseEvent(element, mouseX, mouseY, nativeMouseMoveEventMsg(), testDriver);
  // mouse down
  yield synthesizeNativeMouseEvent(element, mouseX, mouseY, nativeMouseDownEventMsg(), testDriver);
  // drag vertically by |increment| until we reach the specified distance
  for (var y = increment; y < distance; y += increment) {
    yield synthesizeNativeMouseEvent(element, mouseX, mouseY + y, nativeMouseMoveEventMsg(), testDriver);
  }
  yield synthesizeNativeMouseEvent(element, mouseX, mouseY + distance, nativeMouseMoveEventMsg(), testDriver);

  // and return a generator to call afterwards to finish up the drag
  return function*() {
    dump("Finishing drag of #" + element.id + "\n");
    yield synthesizeNativeMouseEvent(element, mouseX, mouseY + distance, nativeMouseUpEventMsg(), testDriver);
  };
}
