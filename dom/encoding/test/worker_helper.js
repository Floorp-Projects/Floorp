/*
 * worker_helper.js
 * bug 764234 tests
*/
function runTestInWorker(files) {
  function workerRun() {
    var tests = [];
    var asserts;
    test = function(func, msg) {
      asserts = [];
      tests.push({asserts: asserts, msg: msg});
    }
    assert_equals = function(result, expected, msg) {
      asserts.push(["assert_equals", result, expected, msg]);
    };
    assert_true = function(condition, msg) {
      asserts.push(["assert_true", condition, msg]);
    };
    assert_unreached = function(condition, msg) {
      asserts.push(["assert_unreached", condition, msg]);
    };
    onmessage = function(event) {
      importScripts.apply(self, event.data);
      runTest();
      postMessage(tests);
    };
  }

  var url = URL.createObjectURL(new Blob([
    runTest.toString(), "\n\n",
    "(", workerRun.toString(), ")();"
  ]));
  var worker = new Worker(url);
  var base = location.toString().replace(/\/[^\/]*$/,"/");
  worker.postMessage(files.map(function(f) { return base + f; }));
  worker.onmessage = function(event) {
    URL.revokeObjectURL(url);
    event.data.forEach(function(t) {
      test(function() {
        t.asserts.forEach(function(a) {
          func = a.shift();
          self[func].apply(self, a);
        });
      }, "worker " + t.msg);
    });
    done();
  };
}
