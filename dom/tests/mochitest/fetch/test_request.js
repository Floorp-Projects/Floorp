function testDefaultCtor() {
  var req = new Request("");
  is(req.method, "GET", "Default Request method is GET");
  ok(req.headers instanceof Headers, "Request should have non-null Headers object");
  is(req.url, self.location.href, "URL should be resolved with entry settings object's API base URL");
  is(req.context, "fetch", "Default context is fetch.");
  is(req.referrer, "about:client", "Default referrer is `client` which serializes to about:client.");
  is(req.mode, "cors", "Request mode for string input is cors");
  is(req.credentials, "omit", "Default Request credentials is omit");
  is(req.cache, "default", "Default Request cache is default");

  var req = new Request(req);
  is(req.method, "GET", "Default Request method is GET");
  ok(req.headers instanceof Headers, "Request should have non-null Headers object");
  is(req.url, self.location.href, "URL should be resolved with entry settings object's API base URL");
  is(req.context, "fetch", "Default context is fetch.");
  is(req.referrer, "about:client", "Default referrer is `client` which serializes to about:client.");
  is(req.mode, "cors", "Request mode string input is cors");
  is(req.credentials, "omit", "Default Request credentials is omit");
  is(req.cache, "default", "Default Request cache is default");
}

function testClone() {
  var orig = new Request("./cloned_request.txt", {
              method: 'POST',
              headers: { "Sample-Header": "5" },
              body: "Sample body",
              mode: "same-origin",
              credentials: "same-origin",
              cache: "no-store",
            });
  var clone = orig.clone();
  ok(clone.method === "POST", "Request method is POST");
  ok(clone.headers instanceof Headers, "Request should have non-null Headers object");

  is(clone.headers.get('sample-header'), "5", "Request sample-header should be 5.");
  orig.headers.set('sample-header', 6);
  is(clone.headers.get('sample-header'), "5", "Cloned Request sample-header should continue to be 5.");

  ok(clone.url === (new URL("./cloned_request.txt", self.location.href)).href,
       "URL should be resolved with entry settings object's API base URL");
  ok(clone.referrer === "about:client", "Default referrer is `client` which serializes to about:client.");
  ok(clone.mode === "same-origin", "Request mode is same-origin");
  ok(clone.credentials === "same-origin", "Default credentials is same-origin");
  ok(clone.cache === "no-store", "Default cache is no-store");

  ok(!orig.bodyUsed, "Original body is not consumed.");
  ok(!clone.bodyUsed, "Clone body is not consumed.");

  var origBody = null;
  var clone2 = null;
  return orig.text().then(function (body) {
    origBody = body;
    is(origBody, "Sample body", "Original body string matches");
    ok(orig.bodyUsed, "Original body is consumed.");
    ok(!clone.bodyUsed, "Clone body is not consumed.");

    try {
      orig.clone()
      ok(false, "Cannot clone Request whose body is already consumed");
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
      ok(false, "Cannot clone Request whose body is already consumed");
    } catch (e) {
      is(e.name, "TypeError", "clone() of consumed body should throw TypeError");
    }

    return clone2.text();
  }).then(function (body) {
    is(body, origBody, "Clone body matches original body.");
    ok(clone2.bodyUsed, "Clone body is consumed.");

    try {
      clone2.clone()
      ok(false, "Cannot clone Request whose body is already consumed");
    } catch (e) {
      is(e.name, "TypeError", "clone() of consumed body should throw TypeError");
    }
  });
}

function testUsedRequest() {
  // Passing a used request should fail.
  var req = new Request("", { method: 'post', body: "This is foo" });
  var p1 = req.text().then(function(v) {
    try {
      var req2 = new Request(req);
      ok(false, "Used Request cannot be passed to new Request");
    } catch(e) {
      ok(true, "Used Request cannot be passed to new Request");
    }
  });

  // Passing a request should set the request as used.
  var reqA = new Request("", { method: 'post', body: "This is foo" });
  var reqB = new Request(reqA);
  is(reqA.bodyUsed, true, "Passing a Request to another Request should set the former as used");
  return p1;
}

function testSimpleUrlParse() {
  // Just checks that the URL parser is actually being used.
  var req = new Request("/file.html");
  is(req.url, (new URL("/file.html", self.location.href)).href, "URL parser should be used to resolve Request URL");
}

// Bug 1109574 - Passing a Request with null body should keep bodyUsed unset.
function testBug1109574() {
  var r1 = new Request("");
  is(r1.bodyUsed, false, "Initial value of bodyUsed should be false");
  var r2 = new Request(r1);
  is(r1.bodyUsed, false, "Request with null body should not have bodyUsed set");
  // This should succeed.
  var r3 = new Request(r1);
}

function testHeaderGuard() {
  var headers = {
    "Cookie": "Custom cookie",
    "Non-Simple-Header": "value",
  };
  var r1 = new Request("", { headers: headers });
  ok(!r1.headers.has("Cookie"), "Default Request header should have guard request and prevent setting forbidden header.");
  ok(r1.headers.has("Non-Simple-Header"), "Default Request header should have guard request and allow setting non-simple header.");

  var r2 = new Request("", { mode: "no-cors", headers: headers });
  ok(!r2.headers.has("Cookie"), "no-cors Request header should have guard request-no-cors and prevent setting non-simple header.");
  ok(!r2.headers.has("Non-Simple-Header"), "no-cors Request header should have guard request-no-cors and prevent setting non-simple header.");
}

function testMethod() {
  // These get normalized.
  var allowed = ["delete", "get", "head", "options", "post", "put" ];
  for (var i = 0; i < allowed.length; ++i) {
    try {
      var r = new Request("", { method: allowed[i] });
      ok(true, "Method " + allowed[i] + " should be allowed");
      is(r.method, allowed[i].toUpperCase(),
         "Standard HTTP method " + allowed[i] + " should be normalized");
    } catch(e) {
      ok(false, "Method " + allowed[i] + " should be allowed");
    }
  }

  var allowed = [ "pAtCh", "foo" ];
  for (var i = 0; i < allowed.length; ++i) {
    try {
      var r = new Request("", { method: allowed[i] });
      ok(true, "Method " + allowed[i] + " should be allowed");
      is(r.method, allowed[i],
         "Non-standard but valid HTTP method " + allowed[i] +
         " should not be normalized");
    } catch(e) {
      ok(false, "Method " + allowed[i] + " should be allowed");
    }
  }

  var forbidden = ["connect", "trace", "track", "<invalid token??"];
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

  // HEAD/GET requests cannot have a body.
  try {
    var r = new Request("", { method: "get", body: "hello" });
    ok(false, "HEAD/GET request cannot have a body");
  } catch(e) {
    is(e.name, "TypeError", "HEAD/GET request cannot have a body");
  }

  try {
    var r = new Request("", { method: "head", body: "hello" });
    ok(false, "HEAD/GET request cannot have a body");
  } catch(e) {
    is(e.name, "TypeError", "HEAD/GET request cannot have a body");
  }

  // Non HEAD/GET should not throw.
  var r = new Request("", { method: "patch", body: "hello" });
}

function testUrlFragment() {
  var req = new Request("./request#withfragment");
  ok(req.url, (new URL("./request", self.location.href)).href, "request.url should be serialized with exclude fragment flag set");
}

function testBodyUsed() {
  var req = new Request("./bodyused", { method: 'post', body: "Sample body" });
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

var text = "κόσμε";
function testBodyCreation() {
  var req1 = new Request("", { method: 'post', body: text });
  var p1 = req1.text().then(function(v) {
    ok(typeof v === "string", "Should resolve to string");
    is(text, v, "Extracted string should match");
  });

  var req2 = new Request("", { method: 'post', body: new Uint8Array([72, 101, 108, 108, 111]) });
  var p2 = req2.text().then(function(v) {
    is("Hello", v, "Extracted string should match");
  });

  var req2b = new Request("", { method: 'post', body: (new Uint8Array([72, 101, 108, 108, 111])).buffer });
  var p2b = req2b.text().then(function(v) {
    is("Hello", v, "Extracted string should match");
  });

  var reqblob = new Request("", { method: 'post', body: new Blob([text]) });
  var pblob = reqblob.text().then(function(v) {
    is(v, text, "Extracted string should match");
  });

  // FormData has its own function since it has blobs and files.

  var params = new URLSearchParams();
  params.append("item", "Geckos");
  params.append("feature", "stickyfeet");
  params.append("quantity", "700");
  var req3 = new Request("", { method: 'post', body: params });
  var p3 = req3.text().then(function(v) {
    var extracted = new URLSearchParams(v);
    is(extracted.get("item"), "Geckos", "Param should match");
    is(extracted.get("feature"), "stickyfeet", "Param should match");
    is(extracted.get("quantity"), "700", "Param should match");
  });

  return Promise.all([p1, p2, p2b, pblob, p3]);
}

function testFormDataBodyCreation() {
  var f1 = new FormData();
  f1.append("key", "value");
  f1.append("foo", "bar");

  var r1 = new Request("", { method: 'post', body: f1 })
  // Since f1 is serialized immediately, later additions should not show up.
  f1.append("more", "stuff");
  var p1 = r1.formData().then(function(fd) {
    ok(fd instanceof FormData, "Valid FormData extracted.");
    ok(fd.has("key"), "key should exist.");
    ok(fd.has("foo"), "foo should exist.");
    ok(!fd.has("more"), "more should not exist.");
  });

  f1.append("blob", new Blob([text]));
  var r2 = new Request("", { method: 'post', body: f1 });
  f1.delete("key");
  var p2 = r2.formData().then(function(fd) {
    ok(fd instanceof FormData, "Valid FormData extracted.");
    ok(fd.has("more"), "more should exist.");

    var b = fd.get("blob");
    ok(b instanceof Blob, "blob entry should be a Blob.");

    return readAsText(b).then(function(output) {
      is(output, text, "Blob contents should match.");
    });
  });

  return Promise.all([p1, p2]);
}

function testBodyExtraction() {
  var text = "κόσμε";
  var newReq = function() { return new Request("", { method: 'post', body: text }); }
  return newReq().text().then(function(v) {
    ok(typeof v === "string", "Should resolve to string");
    is(text, v, "Extracted string should match");
  }).then(function() {
    return newReq().blob().then(function(v) {
      ok(v instanceof Blob, "Should resolve to Blob");
      return readAsText(v).then(function(result) {
        is(result, text, "Decoded Blob should match original");
      });
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
  }).then(function() {
    return newReq().formData().then(function(v) {
      ok(false, "invalid FormData read should fail.");
    }, function(e) {
      ok(e.name == "TypeError", "invalid FormData read should fail.");
    });
  });
}

function testFormDataBodyExtraction() {
  // URLSearchParams translates to application/x-www-form-urlencoded.
  var params = new URLSearchParams();
  params.append("item", "Geckos");
  params.append("feature", "stickyfeet");
  params.append("quantity", "700");
  params.append("quantity", "800");

  var req = new Request("", { method: 'POST', body: params });
  var p1 = req.formData().then(function(fd) {
    ok(fd.has("item"), "Has entry 'item'.");
    ok(fd.has("feature"), "Has entry 'feature'.");
    var entries = fd.getAll("quantity");
    is(entries.length, 2, "Entries with same name are correctly handled.");
    is(entries[0], "700", "Entries with same name are correctly handled.");
    is(entries[1], "800", "Entries with same name are correctly handled.");
  });

  var f1 = new FormData();
  f1.append("key", "value");
  f1.append("foo", "bar");
  f1.append("blob", new Blob([text]));
  var r2 = new Request("", { method: 'post', body: f1 });
  var p2 = r2.formData().then(function(fd) {
    ok(fd.has("key"), "Has entry 'key'.");
    ok(fd.has("foo"), "Has entry 'foo'.");
    ok(fd.has("blob"), "Has entry 'blob'.");
    var entries = fd.getAll("blob");
    is(entries.length, 1, "getAll returns all items.");
    is(entries[0].name, "blob", "Filename should be blob.");
  });

  var ws = "\r\n\r\n\r\n\r\n";
  f1.set("key", new File([ws], 'file name has spaces.txt', { type: 'new/lines' }));
  var r3 = new Request("", { method: 'post', body: f1 });
  var p3 = r3.formData().then(function(fd) {
    ok(fd.has("foo"), "Has entry 'foo'.");
    ok(fd.has("blob"), "Has entry 'blob'.");
    var entries = fd.getAll("blob");
    is(entries.length, 1, "getAll returns all items.");
    is(entries[0].name, "blob", "Filename should be blob.");

    ok(fd.has("key"), "Has entry 'key'.");
    var f = fd.get("key");
    ok(f instanceof File, "entry should be a File.");
    is(f.name, "file name has spaces.txt", "File name should match.");
    is(f.type, "new/lines", "File type should match.");
    is(f.size, ws.length, "File size should match.");
    return readAsText(f).then(function(text) {
      is(text, ws, "File contents should match.");
    });
  });

  // Override header and ensure parse fails.
  var boundary = "1234567891011121314151617";
  var body = boundary +
             '\r\nContent-Disposition: form-data; name="greeting"\r\n\r\n"hello"\r\n' +
             boundary + '-';

  var r4 = new Request("", { method: 'post', body: body, headers: {
                               "Content-Type": "multipart/form-datafoobar; boundary="+boundary,
                           }});
  var p4 = r4.formData().then(function() {
    ok(false, "Invalid mimetype should fail.");
  }, function() {
    ok(true, "Invalid mimetype should fail.");
  });

  var r5 = new Request("", { method: 'POST', body: params, headers: {
                               "Content-Type": "application/x-www-form-urlencodedfoobar",
                           }});
  var p5 = r5.formData().then(function() {
    ok(false, "Invalid mimetype should fail.");
  }, function() {
    ok(true, "Invalid mimetype should fail.");
  });
  return Promise.all([p1, p2, p3, p4]);
}

// mode cannot be set to "CORS-with-forced-preflight" from javascript.
function testModeCorsPreflightEnumValue() {
  try {
    var r = new Request(".", { mode: "cors-with-forced-preflight" });
    ok(false, "Creating Request with mode cors-with-forced-preflight should fail.");
  } catch(e) {
    ok(true, "Creating Request with mode cors-with-forced-preflight should fail.");
    // Also ensure that the error message matches error messages for truly
    // invalid strings.
    var invalidMode = "not-in-requestmode-enum";
    var invalidExc;
    try {
      var r = new Request(".", { mode: invalidMode });
    } catch(e) {
      invalidExc = e;
    }
    var expectedMessage = invalidExc.message.replace(invalidMode, 'cors-with-forced-preflight');
    is(e.message, expectedMessage,
       "mode cors-with-forced-preflight should throw same error as invalid RequestMode strings.");
  }
}

function runTest() {
  testDefaultCtor();
  testSimpleUrlParse();
  testUrlFragment();
  testMethod();
  testBug1109574();
  testHeaderGuard();
  testModeCorsPreflightEnumValue();

  return Promise.resolve()
    .then(testBodyCreation)
    .then(testBodyUsed)
    .then(testBodyExtraction)
    .then(testFormDataBodyCreation)
    .then(testFormDataBodyExtraction)
    .then(testUsedRequest)
    .then(testClone())
    // Put more promise based tests here.
}
