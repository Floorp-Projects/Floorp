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

function testAboutURL() {
  var p1 = fetch('about:blank').then(function(res) {
    is(res.status, 200, "about:blank should load a valid Response");
    is(res.headers.get('content-type'), 'text/html;charset=utf-8',
       "about:blank content-type should be text/html;charset=utf-8");
    return res.text().then(function(v) {
      is(v, "", "about:blank body should be empty");
    });
  });

  var p2 = fetch('about:config').then(function(res) {
    is(res.type, "error", "about:config should fail");
  });

  return Promise.all([p1, p2]);
}

function testDataURL() {
  return fetch("data:text/plain;charset=UTF-8,Hello").then(function(res) {
    ok(true, "Data URL fetch should resolve");
    if (res.type == "error") {
      ok(false, "Data URL fetch should not fail.");
      return Promise.reject();
    }
    ok(res instanceof Response, "Fetch should resolve to a Response");
    is(res.status, 200, "Data URL status should be 200");
    is(res.statusText, "OK", "Data URL statusText should be OK");
    ok(res.headers.has("content-type"), "Headers must have Content-Type header");
    is(res.headers.get("content-type"), "text/plain;charset=UTF-8", "Content-Type header should match specified value");
    return res.text().then(function(body) {
      is(body, "Hello", "Data URL Body should match");
    });
  });
}

function testSameOriginBlobURL() {
  var blob = new Blob(["english ", "sentence"], { type: "text/plain" });
  var url = URL.createObjectURL(blob);
  return fetch(url).then(function(res) {
    ok(true, "Blob URL fetch should resolve");
    if (res.type == "error") {
      ok(false, "Blob URL fetch should not fail.");
      return Promise.reject();
    }
    ok(res instanceof Response, "Fetch should resolve to a Response");
    is(res.status, 200, "Blob fetch status should be 200");
    is(res.statusText, "OK", "Blob fetch statusText should be OK");
    ok(res.headers.has("content-type"), "Headers must have Content-Type header");
    is(res.headers.get("content-type"), blob.type, "Content-Type header should match specified value");
    ok(res.headers.has("content-length"), "Headers must have Content-Length header");
    is(parseInt(res.headers.get("content-length")), 16, "Content-Length should match Blob's size");
    return res.text().then(function(body) {
      is(body, "english sentence", "Blob fetch body should match");
    });
  });
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
    .then(testAboutURL)
    .then(testDataURL)
    .then(testSameOriginBlobURL)
    //.then(testAboutURL)
    // Put more promise based tests here.
    .then(done)
    .catch(function(e) {
      ok(false, "Some Response tests failed " + e);
      done();
    })
}

onmessage = runTest;
