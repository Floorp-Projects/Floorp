// Utilities for synthesizing of native events.

function getPlatform() {
  if (navigator.platform.indexOf("Win") == 0) {
    return "windows";
  }
  if (navigator.platform.indexOf("Mac") == 0) {
    return "mac";
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

function nativeMouseMoveEventMsg() {
  switch (getPlatform()) {
    case "windows": return 1; // MOUSEEVENTF_MOVE
    case "mac": return 5; // NSMouseMoved
    case "linux": return 3; // GDK_MOTION_NOTIFY
  }
  throw "Native wheel events not supported on platform " + getPlatform();
}

// Synthesizes a native mousewheel event and returns immediately. This does not
// guarantee anything; you probably want to use one of the other functions below
// which actually wait for results.
// aX and aY are relative to the top-left of |aElement|'s containing window.
// aDeltaX and aDeltaY are pixel deltas, and aObserver can be left undefined
// if not needed.
function synthesizeNativeWheel(aElement, aX, aY, aDeltaX, aDeltaY, aObserver) {
  var targetWindow = aElement.ownerDocument.defaultView;
  aX += targetWindow.mozInnerScreenX;
  aY += targetWindow.mozInnerScreenY;
  if (aDeltaX && aDeltaY) {
    throw "Simultaneous wheeling of horizontal and vertical is not supported on all platforms.";
  }
  var msg = aDeltaX ? nativeHorizontalWheelEventMsg() : nativeVerticalWheelEventMsg();
  _getDOMWindowUtils().sendNativeMouseScrollEvent(aX, aY, msg, aDeltaX, aDeltaY, 0, 0, 0, aElement, aObserver);
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
  var targetWindow = aElement.ownerDocument.defaultView;
  aX += targetWindow.mozInnerScreenX;
  aY += targetWindow.mozInnerScreenY;
  _getDOMWindowUtils().sendNativeMouseEvent(aX, aY, nativeMouseMoveEventMsg(), 0, aElement);
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
