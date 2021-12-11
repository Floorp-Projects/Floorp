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
  dump("[POINTEREVENT] Starting " + testName + "\n");
};

SimpleTest.finish = function() {
  dump("[POINTEREVENT] Finishing " + testName + "\n");
  opener.nextTest();
};

// Utility functions
function waitForEvent(aTarget, aEvent, aCallback) {
  return new Promise(aResolve => {
    aTarget.addEventListener(
      aEvent,
      e => {
        ok(true, `got ${e.type} event on ${e.target.id}`);
        if (aCallback) {
          aCallback(e);
        }
        aResolve();
      },
      { once: true }
    );
  });
}
