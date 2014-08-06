SimpleTest.requestLongerTimeout(3);
var manager = new MediaTestManager;

// https://bugzilla.mozilla.org/show_bug.cgi?id=634747
if (navigator.platform.startsWith("Win")) {
  SimpleTest.expectAssertions(0, 5);
} else {
  // This is "###!!! ASSERTION: Page read cursor should be inside range: 'mPageOffset <= endOffset'"
  // https://bugzilla.mozilla.org/show_bug.cgi?id=846769
  SimpleTest.expectAssertions(0, 5);
}

function createTestArray() {
  var tests = [];
  var tmpVid = document.createElement("video");

  for (var testNum=0; testNum<gSeekTests.length; testNum++) {
    var test = gSeekTests[testNum];
    if (!tmpVid.canPlayType(test.type)) {
      continue;
    }

    var t = new Object;
    t.name = test.name;
    t.type = test.type;
    t.duration = test.duration;
    t.number = SEEK_TEST_NUMBER;
    tests.push(t);
  }
  return tests;
}

function startTest(test, token) {
  var v = document.createElement('video');
  v.token = token += "-seek" + test.number + ".js";
  manager.started(v.token);
  v.src = test.name;
  v.preload = "metadata";
  document.body.appendChild(v);
  var name = test.name + " seek test " + test.number;
  var localIs = function(name) { return function(a, b, msg) {
    is(a, b, name + ": " + msg);
  }}(name);
  var localOk = function(name) { return function(a, msg) {
    ok(a, name + ": " + msg);
  }}(name);
  var localFinish = function(v, manager) { return function() {
    v.onerror = null;
    removeNodeAndSource(v);
    dump("SEEK-TEST: Finished " + name + " token: " + v.token + "\n");
    manager.finished(v.token);
  }}(v, manager);
  dump("SEEK-TEST: Started " + name + "\n");
  window['test_seek' + test.number](v, test.duration/2, localIs, localOk, localFinish);
}
