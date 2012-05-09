const isWinXP = navigator.userAgent.indexOf("Windows NT 5.1") != -1;
const isOSXLion = navigator.userAgent.indexOf("Mac OS X 10.7") != -1;

// If we're running in a child window, shim things so it works the same
// as if we were running stand-alone.
if (window.opener) {
  // Get test filename for page being run in popup so errors are more useful
  var testName = location.pathname.split('/').pop();

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

  // Override bits of SimpleTest so test files work stand-alone
  var SimpleTest = SimpleTest || {};

  SimpleTest.waitForExplicitFinish = function() {
    dump("[POINTERLOCK] Starting " + testName+ "\n");
  };

  SimpleTest.finish = function () {
    dump("[POINTERLOCK] Finishing " + testName+ "\n");
    opener.nextTest();
  };
} else {
  // If we're not running in a child window, prefs need to get flipped here,
  // otherwise it was already done in the test runner parent.

  // Ensure the full-screen api is enabled, and will be disabled on test exit.
  SpecialPowers.setBoolPref("full-screen-api.enabled", true);

  // Disable the requirement for trusted contexts only, so the tests are easier to write.
  SpecialPowers.setBoolPref("full-screen-api.allow-trusted-requests-only", false);
}

addLoadEvent(function() {
  SimpleTest.waitForFocus(start);
});
