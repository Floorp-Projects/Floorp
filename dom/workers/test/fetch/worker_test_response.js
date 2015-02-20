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
  var orig = new Response("This is a body", {
              status: 404,
              statusText: "Not Found",
              headers: { "Content-Length": 5 },
            });
  var clone = orig.clone();
  is(clone.status, 404, "Response status is 404");
  is(clone.statusText, "Not Found", "Response statusText is POST");
  ok(clone.headers instanceof Headers, "Response should have non-null Headers object");

  is(clone.headers.get('content-length'), "5", "Response content-length should be 5.");
  orig.headers.set('content-length', 6);
  is(clone.headers.get('content-length'), "5", "Response content-length should be 5.");

  ok(!orig.bodyUsed, "Original body is not consumed.");
  ok(!clone.bodyUsed, "Clone body is not consumed.");

  var origBody = null;
  var clone2 = null;
  return orig.text().then(function (body) {
    origBody = body;
    is(origBody, "This is a body", "Original body string matches");
    ok(orig.bodyUsed, "Original body is consumed.");
    ok(!clone.bodyUsed, "Clone body is not consumed.");

    try {
      orig.clone()
      ok(false, "Cannot clone Response whose body is already consumed");
    } catch (e) {
      is(e.name, "TypeError", "clone() of consumed body should throw TypeError");
    }

    clone2 = clone.clone();
    return clone.text();
  }).then(function (body) {
    is(body, origBody, "Clone body matches original body.");
    ok(clone.bodyUsed, "Clone body is consumed.");

    try {
      clone.clone()
      ok(false, "Cannot clone Response whose body is already consumed");
    } catch (e) {
      is(e.name, "TypeError", "clone() of consumed body should throw TypeError");
    }

    return clone2.text();
  }).then(function (body) {
    is(body, origBody, "Clone body matches original body.");
    ok(clone2.bodyUsed, "Clone body is consumed.");

    try {
      clone2.clone()
      ok(false, "Cannot clone Response whose body is already consumed");
    } catch (e) {
      is(e.name, "TypeError", "clone() of consumed body should throw TypeError");
    }
  });
}

function testRedirect() {
  var res = Response.redirect("./redirect.response");
  is(res.status, 302, "Default redirect has status code 302");
  var h = res.headers.get("location");
  ok(h === (new URL("./redirect.response", self.location.href)).href, "Location header should be correct absolute URL");

  var successStatus = [301, 302, 303, 307, 308];
  for (var i = 0; i < successStatus.length; ++i) {
    var res = Response.redirect("./redirect.response", successStatus[i]);
    is(res.status, successStatus[i], "Status code should match");
  }

  var failStatus = [300, 0, 304, 305, 306, 309, 500];
  for (var i = 0; i < failStatus.length; ++i) {
    try {
      var res = Response.redirect(".", failStatus[i]);
      ok(false, "Invalid status code should fail " + failStatus[i]);
    } catch(e) {
      is(e.name, "RangeError", "Invalid status code should fail " + failStatus[i]);
    }
  }
}

function testOk() {
  var r1 = new Response("", { status: 200});
  ok(r1.ok, "Response with status 200 should have ok true");

  var r2 = new Response("", { status: 204});
  ok(r2.ok, "Response with status 204 should have ok true");

  var r3 = new Response("", { status: 299});
  ok(r3.ok, "Response with status 299 should have ok true");

  var r4 = new Response("", { status: 302});
  ok(!r4.ok, "Response with status 302 should have ok false");
}

// It is not possible to test setting finalURL until we have ServiceWorker
// interception. This is because synthetic Responses do not have a url, the url
// is set based on the request, so a SW could initiate a fetch() on behalf of
// a client and set the resulting Response's finalURL before returning it to
// the client, in which case the "set response's url to request's url" from the
// client's point of view would not happen. A test for this will be added by
// Bug 1134352.
function testFinalURL() {
  var r1 = new Response();
  ok(!r1.finalURL, "Response.finalURL is false by default.");

  try {
    r1.finalURL = true;
    ok(false, "Setting Response.finalURL of Response with null url should fail.");
  } catch(e) {
    ok(true, "Setting Response.finalURL of Response with null url should fail.");
  }
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

  var resblob = new Response(new Blob([text]));
  var pblob = resblob.text().then(function(v) {
    is(v, text, "Extracted string should match");
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

  return Promise.all([p1, p2, p2b, pblob, p3]);
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
  testRedirect();
  testOk();
  testFinalURL();

  Promise.resolve()
    .then(testBodyCreation)
    .then(testBodyUsed)
    .then(testBodyExtraction)
    .then(testClone)
    // Put more promise based tests here.
    .then(done)
    .catch(function(e) {
      ok(false, "Some Response tests failed " + e);
      done();
    })
}
