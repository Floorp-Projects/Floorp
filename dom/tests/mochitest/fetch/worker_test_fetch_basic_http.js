if (typeof ok !== "function") {
  function ok(a, msg) {
    dump("OK: " + !!a + "  =>  " + a + " " + msg + "\n");
    postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
  }
}

if (typeof is !== "function") {
  function is(a, b, msg) {
    dump("IS: " + (a===b) + "  =>  " + a + " | " + b + " " + msg + "\n");
    postMessage({type: 'status', status: a === b, msg: a + " === " + b + ": " + msg });
  }
}

var path = "/tests/dom/base/test/";

var passFiles = [['file_XHR_pass1.xml', 'GET', 200, 'OK', 'text/xml'],
                 ['file_XHR_pass2.txt', 'GET', 200, 'OK', 'text/plain'],
                 ['file_XHR_pass3.txt', 'GET', 200, 'OK', 'text/plain'],
                 ];

function testURL() {
  var promises = [];
  passFiles.forEach(function(entry) {
    var p = fetch(path + entry[0]).then(function(res) {
      ok(res.type !== "error", "Response should not be an error for " + entry[0]);
      is(res.status, entry[2], "Status should match expected for " + entry[0]);
      is(res.statusText, entry[3], "Status text should match expected for " + entry[0]);
      ok(res.url.endsWith(path + entry[0]), "Response url should match request for simple fetch for " + entry[0]);
      is(res.headers.get('content-type'), entry[4], "Response should have content-type for " + entry[0]);
    });
    promises.push(p);
  });

  return Promise.all(promises);
}

var failFiles = [['ftp://localhost' + path + 'file_XHR_pass1.xml', 'GET']];

function testURLFail() {
  var promises = [];
  failFiles.forEach(function(entry) {
    var p = fetch(entry[0]).then(function(res) {
      ok(res.type === "error", "Response should be an error for " + entry[0]);
      is(res.status, 0, "Response status should be 0 for " + entry[0]);
    });
    promises.push(p);
  });

  return Promise.all(promises);
}

function testRequestGET() {
  var promises = [];
  passFiles.forEach(function(entry) {
    var req = new Request(path + entry[0], { method: entry[1] });
    var p = fetch(req).then(function(res) {
      ok(res.type !== "error", "Response should not be an error for " + entry[0]);
      is(res.status, entry[2], "Status should match expected for " + entry[0]);
      is(res.statusText, entry[3], "Status text should match expected for " + entry[0]);
      ok(res.url.endsWith(path + entry[0]), "Response url should match request for simple fetch for " + entry[0]);
      is(res.headers.get('content-type'), entry[4], "Response should have content-type for " + entry[0]);
    });
    promises.push(p);
  });

  return Promise.all(promises);
}

function runTest() {
  var done = function() {
    if (typeof SimpleTest === "object") {
      SimpleTest.finish();
    } else {
      postMessage({ type: 'finish' });
    }
  }

  Promise.resolve()
    .then(testURL)
    .then(testURLFail)
    .then(testRequestGET)
    // Put more promise based tests here.
    .then(done)
    .catch(function(e) {
      ok(false, "Some Response tests failed " + e);
      done();
    })
}

onmessage = runTest;
