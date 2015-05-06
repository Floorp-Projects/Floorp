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

// Synthesizes a native mousewheel event and returns immediately. This does not
// guarantee anything; you probably want to use one of the other functions below
// which actually wait for results.
// aX and aY are relative to |window|'s top-left. aDeltaX and aDeltaY
// are pixel deltas, and aObserver can be left undefined if not needed.
function synthesizeNativeWheel(aElement, aX, aY, aDeltaX, aDeltaY, aObserver) {
  aX += window.mozInnerScreenX;
  aY += window.mozInnerScreenY;
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
// wheel event is dispatched to the window. See synthesizeNativeWheel for
// details on the parameters.
function synthesizeNativeWheelAndWaitForEvent(aElement, aX, aY, aDeltaX, aDeltaY, aCallback) {
  window.addEventListener("wheel", function wheelWaiter(e) {
    window.removeEventListener("wheel", wheelWaiter);
    setTimeout(aCallback, 0);
  });
  return synthesizeNativeWheel(aElement, aX, aY, aDeltaX, aDeltaY);
}
