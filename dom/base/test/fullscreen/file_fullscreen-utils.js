// Keep track of how many fullscreenChange enters we've received, so that
// we can balance them with the number of exits we receive. We reset this
// to 0 when we load a test.
var fullscreenChangeEnters = 0;

addLoadEvent(function () {
  info(`Resetting fullscreen enter count.`);
  fullscreenChangeEnters = 0;
});

// This can be used to force a certain value for fullscreenChangeEnters
// to handle unusual conditions -- such as exiting multiple levels of
// fullscreen forcibly.
function setFullscreenChangeEnters(enters) {
  info(`Setting fullscreen enter count to ${enters}.`);
  fullscreenChangeEnters = enters;
}

// Returns true if the window believes it is in fullscreen. This may be true even
// before an asynchronous fullscreen transition is complete.
function inFullscreenMode(win) {
  return win.document.fullscreenElement;
}

// Adds a listener that will be called once a fullscreen transition
// is complete. When type==='enter', callback is called when we've
// received a fullscreenchange event, and the fullscreen transition is
// complete. When type==='exit', callback is called when we've
// received a fullscreenchange event and the window is out of
// fullscreen. inDoc is the document which the listeners are added on,
// if absent, the listeners are added to the current document.
// the current document.
function addFullscreenChangeContinuation(type, callback, inDoc) {
  var doc = inDoc || document;
  var topWin = doc.defaultView.top;
  function checkCondition() {
    if (type == "enter") {
      fullscreenChangeEnters++;
      return inFullscreenMode(topWin);
    } else if (type == "exit") {
      fullscreenChangeEnters--;
      return fullscreenChangeEnters
        ? inFullscreenMode(topWin)
        : !inFullscreenMode(topWin);
    }
    throw new Error("'type' must be either 'enter', or 'exit'.");
  }
  function onFullscreenChange(event) {
    doc.removeEventListener("fullscreenchange", onFullscreenChange);
    ok(checkCondition(), `Should ${type} fullscreen.`);
    // Delay invocation so other listeners have a chance to respond before
    // we continue.
    requestAnimationFrame(() => setTimeout(() => callback(event), 0), 0);
  }
  doc.addEventListener("fullscreenchange", onFullscreenChange);
}

// Calls |callback| when the next fullscreenerror is dispatched to inDoc||document.
function addFullscreenErrorContinuation(callback, inDoc) {
  let doc = inDoc || document;
  let listener = function (event) {
    doc.removeEventListener("fullscreenerror", listener);
    // Delay invocation so other listeners have a chance to respond before
    // we continue.
    requestAnimationFrame(() => setTimeout(() => callback(event), 0), 0);
  };
  doc.addEventListener("fullscreenerror", listener);
}

// Waits until the window has both the load event and a MozAfterPaint called on
// it, and then invokes the callback
function waitForLoadAndPaint(win, callback) {
  win.addEventListener(
    "MozAfterPaint",
    function () {
      // The load event may have fired before the MozAfterPaint, in which case
      // listening for it now will hang. Instead we check the readyState to see if
      // it already fired, and if so, invoke the callback right away.
      if (win.document.readyState == "complete") {
        callback();
      } else {
        win.addEventListener("load", callback, { once: true });
      }
    },
    { once: true }
  );
}
