// Get test filename for page being run in popup so errors are more useful
var testName = location.pathname.split("/").pop();

// Wrap test functions and pass to parent window
window.ok = function(a, msg) {
  opener.ok(a, testName + ": " + msg);
};

window.is = function(a, b, msg) {
  opener.is(a, b, testName + ": " + msg);
};

window.isnot = function(a, b, msg) {
  opener.isnot(a, b, testName + ": " + msg);
};

window.todo = function(a, msg) {
  opener.todo(a, testName + ": " + msg);
};

window.todo_is = function(a, b, msg) {
  opener.todo_is(a, b, testName + ": " + msg);
};

window.todo_isnot = function(a, b, msg) {
  opener.todo_isnot(a, b, testName + ": " + msg);
};

window.info = function(msg) {
  opener.info(testName + ": " + msg);
};

// Override bits of SimpleTest so test files work stand-alone
var SimpleTest = SimpleTest || {};

SimpleTest.waitForExplicitFinish = function() {
  dump("[POINTERLOCK] Starting " + testName + "\n");
};

SimpleTest.finish = function() {
  dump("[POINTERLOCK] Finishing " + testName + "\n");
  opener.nextTest();
};

addLoadEvent(function() {
  if (typeof start !== "undefined") {
    SimpleTest.waitForFocus(start);
  }
});

// Returns true if the window occupies the entire screen.
// Note this only returns true once the transition from normal to
// fullscreen mode is complete.
function inFullscreenMode(win) {
  return (
    win.innerWidth == win.screen.width && win.innerHeight == win.screen.height
  );
}

// Returns true if the window is in normal mode, i.e. non fullscreen mode.
// Note this only returns true once the transition from fullscreen back to
// normal mode is complete.
function inNormalMode(win) {
  return (
    win.innerWidth == win.normalSize.w && win.innerHeight == win.normalSize.h
  );
}

// Adds a listener that will be called once a fullscreen transition
// is complete. When type==='enter', callback is called when we've
// received a fullscreenchange event, and the fullscreen transition is
// complete. When type==='exit', callback is called when we've
// received a fullscreenchange event and the window dimensions match
// the window dimensions when the window opened (so don't resize the
// window while running your test!). inDoc is the document which
// the listeners are added on, if absent, the listeners are added to
// the current document.
function addFullscreenChangeContinuation(type, callback, inDoc) {
  var doc = inDoc || document;
  var topWin = doc.defaultView.top;
  // Remember the window size in non-fullscreen mode.
  if (!topWin.normalSize) {
    topWin.normalSize = {
      w: window.innerWidth,
      h: window.innerHeight,
    };
  }
  function checkCondition() {
    if (type == "enter") {
      return inFullscreenMode(topWin);
    } else if (type == "exit") {
      // If we just revert the state to a previous fullscreen state,
      // the window won't back to the normal mode. Hence we check
      // fullscreenElement first here. Note that we need to check
      // the fullscreen element of the outmost document here instead
      // of the current one.
      return topWin.document.fullscreenElement || inNormalMode(topWin);
    } else {
      throw "'type' must be either 'enter', or 'exit'.";
    }
  }
  function invokeCallback(event) {
    // Use async call after a paint to workaround unfinished fullscreen
    // change even when the window size has changed on Linux.
    requestAnimationFrame(() => setTimeout(() => callback(event), 0), 0);
  }
  function onFullscreenChange(event) {
    doc.removeEventListener("fullscreenchange", onFullscreenChange);
    if (checkCondition()) {
      invokeCallback(event);
      return;
    }
    function onResize() {
      if (checkCondition()) {
        topWin.removeEventListener("resize", onResize);
        invokeCallback(event);
      }
    }
    topWin.addEventListener("resize", onResize);
  }
  doc.addEventListener("fullscreenchange", onFullscreenChange);
}
