var manager = new MediaTestManager;

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
