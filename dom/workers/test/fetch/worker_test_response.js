function ok(a, msg) {
  dump("OK: " + !!a + "  =>  " + a + " " + msg + "\n");
  postMessage({type: 'status', status: !!a, msg: a + ": " + msg });
}

function is(a, b, msg) {
  dump("IS: " + (a===b) + "  =>  " + a + " | " + b + " " + msg + "\n");
  postMessage({type: 'status', status: a === b, msg: a + " === " + b + ": " + msg });
}

function testDefaultCtor() {
  var res = new Response();
  is(res.type, "default", "Default Response type is default");
  ok(res.headers instanceof Headers, "Response should have non-null Headers object");
  is(res.url, "", "URL should be empty string");
  is(res.status, 200, "Default status is 200");
  is(res.statusText, "OK", "Default statusText is OK");
}

function testClone() {
  var res = (new Response("This is a body", {
              status: 404,
              statusText: "Not Found",
              headers: { "Content-Length": 5 },
            })).clone();
  is(res.status, 404, "Response status is 404");
  is(res.statusText, "Not Found", "Response statusText is POST");
  ok(res.headers instanceof Headers, "Response should have non-null Headers object");
  is(res.headers.get('content-length'), "5", "Response content-length should be 5.");
}

function testBodyUsed() {
  var res = new Response("Sample body");
  ok(!res.bodyUsed, "bodyUsed is initially false.");
  return res.text().then((v) => {
    is(v, "Sample body", "Body should match");
    ok(res.bodyUsed, "After reading body, bodyUsed should be true.");
  }).then(() => {
    return res.blob().then((v) => {
      ok(false, "Attempting to read body again should fail.");
    }, (e) => {
      ok(true, "Attempting to read body again should fail.");
    })
  });
}

// FIXME(nsm): Bug 1071290: We can't use Blobs as the body yet.
function testBodyCreation() {
  var text = "κόσμε";
  var res1 = new Response(text);
  var p1 = res1.text().then(function(v) {
    ok(typeof v === "string", "Should resolve to string");
    is(text, v, "Extracted string should match");
  });

  var res2 = new Response(new Uint8Array([72, 101, 108, 108, 111]));
  var p2 = res2.text().then(function(v) {
    is("Hello", v, "Extracted string should match");
  });

  var res2b = new Response((new Uint8Array([72, 101, 108, 108, 111])).buffer);
  var p2b = res2b.text().then(function(v) {
    is("Hello", v, "Extracted string should match");
  });

  var params = new URLSearchParams();
  params.append("item", "Geckos");
  params.append("feature", "stickyfeet");
  params.append("quantity", "700");
  var res3 = new Response(params);
  var p3 = res3.text().then(function(v) {
    var extracted = new URLSearchParams(v);
    is(extracted.get("item"), "Geckos", "Param should match");
    is(extracted.get("feature"), "stickyfeet", "Param should match");
    is(extracted.get("quantity"), "700", "Param should match");
  });

  return Promise.all([p1, p2, p2b, p3]);
}

function testBodyExtraction() {
  var text = "κόσμε";
  var newRes = function() { return new Response(text); }
  return newRes().text().then(function(v) {
    ok(typeof v === "string", "Should resolve to string");
    is(text, v, "Extracted string should match");
  }).then(function() {
    return newRes().blob().then(function(v) {
      ok(v instanceof Blob, "Should resolve to Blob");
      var fs = new FileReaderSync();
      is(fs.readAsText(v), text, "Decoded Blob should match original");
    });
  }).then(function() {
    return newRes().json().then(function(v) {
      ok(false, "Invalid json should reject");
    }, function(e) {
      ok(true, "Invalid json should reject");
    })
  }).then(function() {
    return newRes().arrayBuffer().then(function(v) {
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

  Promise.resolve()
    .then(testBodyCreation)
    .then(testBodyUsed)
    .then(testBodyExtraction)
    // Put more promise based tests here.
    .then(done)
    .catch(function(e) {
      ok(false, "Some Response tests failed " + e);
      done();
    })
}
