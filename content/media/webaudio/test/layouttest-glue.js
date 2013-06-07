// Reimplementation of the LayoutTest API from Blink so we can easily port
// WebAudio tests to Simpletest, without touching the internals of the test.

function testFailed(msg) {
  ok(false, msg);
}

function testPassed(msg) {
  ok(true, msg);
}

function finishJSTest() {
  SimpleTest.finish();
}

