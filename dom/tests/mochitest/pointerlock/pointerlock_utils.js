// Get test filename for page being run in popup so errors are more useful
var testName = location.pathname.split('/').pop();

// Wrap test functions and pass to parent window
function ok(a, msg) {
  opener.ok(a, testName + ": " + msg);
}

function is(a, b, msg) {
  opener.is(a, b, testName + ": " + msg);
}

function isnot(a, b, msg) {
  opener.isnot(a, b, testName + ": " + msg);
}

function todo(a, msg) {
  opener.todo(a, testName + ": " + msg);
}

function todo_is(a, b, msg) {
  opener.todo_is(a, b, testName + ": " + msg);
}

function todo_isnot(a, b, msg) {
  opener.todo_isnot(a, b, testName + ": " + msg);
}

// Override SimpleTest so test files work stand-alone
SimpleTest.finish = function () {
  opener.nextTest();
};

SimpleTest.waitForExplicitFinish = function() {};
