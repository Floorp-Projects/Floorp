// This file expects manifest.js to be included in the same scope.
/* import-globals-from manifest.js */
// This file expects SEEK_TEST_NUMBER to be defined by the test.
/* global SEEK_TEST_NUMBER */
var manager = new MediaTestManager();

function createTestArray() {
  var tests = [];
  var tmpVid = document.createElement("video");

  for (var testNum = 0; testNum < gSeekTests.length; testNum++) {
    var test = gSeekTests[testNum];
    if (!tmpVid.canPlayType(test.type)) {
      continue;
    }

    var t = {};
    t.name = test.name;
    t.type = test.type;
    t.duration = test.duration;
    t.number = SEEK_TEST_NUMBER;
    tests.push(t);
  }
  return tests;
}

function startTest(test, token) {
  var video = document.createElement("video");
  video.token = token += "-seek" + test.number + ".js";
  manager.started(video.token);
  video.src = test.name;
  video.preload = "metadata";
  document.body.appendChild(video);
  var name = test.name + " seek test " + test.number;
  var localIs = (function (n) {
    return function (a, b, msg) {
      is(a, b, n + ": " + msg);
    };
  })(name);
  var localOk = (function (n) {
    return function (a, msg) {
      ok(a, n + ": " + msg);
    };
  })(name);
  var localFinish = (function (v, m) {
    return function () {
      v.onerror = null;
      removeNodeAndSource(v);
      dump("SEEK-TEST: Finished " + name + " token: " + v.token + "\n");
      m.finished(v.token);
    };
  })(video, manager);
  dump("SEEK-TEST: Started " + name + "\n");
  window["test_seek" + test.number](
    video,
    test.duration / 2,
    localIs,
    localOk,
    localFinish
  );
}
