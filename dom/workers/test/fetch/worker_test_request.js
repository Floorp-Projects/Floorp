function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + " " + msg + "\n");
  postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  dump("IS: " + (a===b) + "  =>  " + a + " | " + b + " " + msg + "\n");
  postMessage({type: 'status', status: a === b, msg: a + " === " + b + ": " + msg });
}

function testDefaultCtor() {
  var req = new Request("");
  is(req.method, "GET", "Default Request method is GET");
  ok(req.headers instanceof Headers, "Request should have non-null Headers object");
  is(req.url, self.location.href, "URL should be resolved with entry settings object's API base URL");
  is(req.referrer, "", "Default referrer is `client` which serializes to empty string.");
  is(req.mode, "cors", "Request mode for string input is cors");
  is(req.credentials, "omit", "Default Request credentials is omit");

  var req = new Request(req);
  is(req.method, "GET", "Default Request method is GET");
  ok(req.headers instanceof Headers, "Request should have non-null Headers object");
  is(req.url, self.location.href, "URL should be resolved with entry settings object's API base URL");
  is(req.referrer, "", "Default referrer is `client` which serializes to empty string.");
  is(req.mode, "cors", "Request mode string input is cors");
  is(req.credentials, "omit", "Default Request credentials is omit");
}

function testClone() {
  var req = (new Request("./cloned_request.txt", {
              method: 'POST',
              headers: { "Content-Length": 5 },
              body: "Sample body",
              mode: "same-origin",
              credentials: "same-origin",
            })).clone();
  ok(req.method === "POST", "Request method is POST");
  ok(req.headers instanceof Headers, "Request should have non-null Headers object");
  is(req.headers.get('content-length'), "5", "Request content-length should be 5.");
  ok(req.url === (new URL("./cloned_request.txt", self.location.href)).href,
       "URL should be resolved with entry settings object's API base URL");
  ok(req.referrer === "", "Default referrer is `client` which serializes to empty string.");
  ok(req.mode === "same-origin", "Request mode is same-origin");
  ok(req.credentials === "same-origin", "Default credentials is same-origin");
}

function testUsedRequest() {
  // Passing a used request should fail.
  var req = new Request("", { body: "This is foo" });
  var p1 = req.text().then(function(v) {
    try {
      var req2 = new Request(req);
      ok(false, "Used Request cannot be passed to new Request");
    } catch(e) {
      ok(true, "Used Request cannot be passed to new Request");
    }
  });

  // Passing a request should set the request as used.
  var reqA = new Request("", { body: "This is foo" });
  var reqB = new Request(reqA);
  is(reqA.bodyUsed, true, "Passing a Request to another Request should set the former as used");
  return p1;
}

function testSimpleUrlParse() {
  // Just checks that the URL parser is actually being used.
  var req = new Request("/file.html");
  is(req.url, (new URL("/file.html", self.location.href)).href, "URL parser should be used to resolve Request URL");
}

function testMethod() {
  var allowed = ["delete", "get", "head", "options", "post", "put"];
  for (var i = 0; i < allowed.length; ++i) {
    try {
      var r = new Request("", { method: allowed[i] });
      ok(true, "Method " + allowed[i] + " should be allowed");
    } catch(e) {
      ok(false, "Method " + allowed[i] + " should be allowed");
    }
  }

  var forbidden = ["aardvark", "connect", "trace", "track"];
  for (var i = 0; i < forbidden.length; ++i) {
    try {
      var r = new Request("", { method: forbidden[i] });
      ok(false, "Method " + forbidden[i] + " should be forbidden");
    } catch(e) {
      ok(true, "Method " + forbidden[i] + " should be forbidden");
    }
  }

  var allowedNoCors = ["get", "head", "post"];
  for (var i = 0; i < allowedNoCors.length; ++i) {
    try {
      var r = new Request("", { method: allowedNoCors[i], mode: "no-cors" });
      ok(true, "Method " + allowedNoCors[i] + " should be allowed in no-cors mode");
    } catch(e) {
      ok(false, "Method " + allowedNoCors[i] + " should be allowed in no-cors mode");
    }
  }

  var forbiddenNoCors = ["aardvark", "delete", "options", "put"];
  for (var i = 0; i < forbiddenNoCors.length; ++i) {
    try {
      var r = new Request("", { method: forbiddenNoCors[i], mode: "no-cors" });
      ok(false, "Method " + forbiddenNoCors[i] + " should be forbidden in no-cors mode");
    } catch(e) {
      ok(true, "Method " + forbiddenNoCors[i] + " should be forbidden in no-cors mode");
    }
  }
}

function testUrlFragment() {
  var req = new Request("./request#withfragment");
  ok(req.url, (new URL("./request", self.location.href)).href, "request.url should be serialized with exclude fragment flag set");
}

function testBodyUsed() {
  var req = new Request("./bodyused", { body: "Sample body" });
  is(req.bodyUsed, false, "bodyUsed is initially false.");
  return req.text().then((v) => {
    is(v, "Sample body", "Body should match");
    is(req.bodyUsed, true, "After reading body, bodyUsed should be true.");
  }).then((v) => {
    return req.blob().then((v) => {
      ok(false, "Attempting to read body again should fail.");
    }, (e) => {
      ok(true, "Attempting to read body again should fail.");
    })
  });
}

function testBodyCreation() {
  var text = "κόσμε";
  var req1 = new Request("", { body: text });
  var p1 = req1.text().then(function(v) {
    ok(typeof v === "string", "Should resolve to string");
    is(text, v, "Extracted string should match");
  });

  var req2 = new Request("", { body: new Uint8Array([72, 101, 108, 108, 111]) });
  var p2 = req2.text().then(function(v) {
    is("Hello", v, "Extracted string should match");
  });

  var req2b = new Request("", { body: (new Uint8Array([72, 101, 108, 108, 111])).buffer });
  var p2b = req2b.text().then(function(v) {
    is("Hello", v, "Extracted string should match");
  });

  var reqblob = new Request("", { body: new Blob([text]) });
  var pblob = reqblob.text().then(function(v) {
    is(v, text, "Extracted string should match");
  });

  var params = new URLSearchParams();
  params.append("item", "Geckos");
  params.append("feature", "stickyfeet");
  params.append("quantity", "700");
  var req3 = new Request("", { body: params });
  var p3 = req3.text().then(function(v) {
    var extracted = new URLSearchParams(v);
    is(extracted.get("item"), "Geckos", "Param should match");
    is(extracted.get("feature"), "stickyfeet", "Param should match");
    is(extracted.get("quantity"), "700", "Param should match");
  });

  return Promise.all([p1, p2, p2b, pblob, p3]);
}

function testBodyExtraction() {
  var text = "κόσμε";
  var newReq = function() { return new Request("", { body: text }); }
  return newReq().text().then(function(v) {
    ok(typeof v === "string", "Should resolve to string");
    is(text, v, "Extracted string should match");
  }).then(function() {
    return newReq().blob().then(function(v) {
      ok(v instanceof Blob, "Should resolve to Blob");
      var fs = new FileReaderSync();
      is(fs.readAsText(v), text, "Decoded Blob should match original");
    });
  }).then(function() {
    return newReq().json().then(function(v) {
      ok(false, "Invalid json should reject");
    }, function(e) {
      ok(true, "Invalid json should reject");
    })
  }).then(function() {
    return newReq().arrayBuffer().then(function(v) {
      ok(v instanceof ArrayBuffer, "Should resolve to ArrayBuffer");
      var dec = new TextDecoder();
      is(dec.decode(new Uint8Array(v)), text, "UTF-8 decoded ArrayBuffer should match original");
    });
  })
}

onmessage = function() {
  var done = function() { postMessage({ type: 'finish' }) }

  testDefaultCtor();
  testClone();
  testSimpleUrlParse();
  testUrlFragment();
  testMethod();

  Promise.resolve()
    .then(testBodyCreation)
    .then(testBodyUsed)
    .then(testBodyExtraction)
    .then(testUsedRequest)
    // Put more promise based tests here.
    .then(done)
    .catch(function(e) {
      ok(false, "Some Request tests failed " + e);
      done();
    })
}
