const { ChromeUtils } = SpecialPowers;
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

// Get test filename for page being run in popup so errors are more useful
var testName = location.pathname.split("/").pop();

// Wrap test functions and pass to parent window
window.ok = function (a, msg) {
  opener.ok(a, testName + ": " + msg);
};

window.is = function (a, b, msg) {
  opener.is(a, b, testName + ": " + msg);
};

window.isnot = function (a, b, msg) {
  opener.isnot(a, b, testName + ": " + msg);
};

window.todo = function (a, msg) {
  opener.todo(a, testName + ": " + msg);
};

window.todo_is = function (a, b, msg) {
  opener.todo_is(a, b, testName + ": " + msg);
};

window.todo_isnot = function (a, b, msg) {
  opener.todo_isnot(a, b, testName + ": " + msg);
};

window.info = function (msg) {
  opener.info(testName + ": " + msg);
};

// Override bits of SimpleTest so test files work stand-alone
var SimpleTest = SimpleTest || {};

SimpleTest.waitForExplicitFinish = function () {
  dump("[POINTERLOCK] Starting " + testName + "\n");
};

SimpleTest.finish = function () {
  dump("[POINTERLOCK] Finishing " + testName + "\n");
  opener.nextTest();
};

// Keep track of how many fullscreenChange enters we've received, so that
// we can balance them with the number of exits we receive. We reset this
// to 0 when we load a test.
var fullscreenChangeEnters = 0;

addLoadEvent(function () {
  info(`Resetting fullscreen enter count.`);
  fullscreenChangeEnters = 0;
  if (typeof start !== "undefined") {
    // Delay one event loop to stabilize the initial state of the page.
    SimpleTest.executeSoon(start);
  }
});

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
    } else {
      throw "'type' must be either 'enter', or 'exit'.";
    }
  }
  function onFullscreenChange(event) {
    doc.removeEventListener("fullscreenchange", onFullscreenChange);
    ok(checkCondition(), `Should ${type} fullscreen.`);

    // Invoke the callback after the browsingContext has become active
    // (or we've timed out waiting for it to become active).
    let bc = SpecialPowers.wrap(topWin).browsingContext;
    TestUtils.waitForCondition(
      () => bc.isActive,
      "browsingContext should become active"
    )
      .catch(e =>
        ok(false, `Wait for browsingContext.isActive failed with ${e}`)
      )
      .finally(() => {
        requestAnimationFrame(() => setTimeout(() => callback(event), 0), 0);
      });
  }
  doc.addEventListener("fullscreenchange", onFullscreenChange);
}
