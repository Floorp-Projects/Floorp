function testAboutURL() {
  var p1 = fetch('about:blank').then(function(res) {
    ok(false, "about:blank should fail");
  }, function(e) {
    ok(e instanceof TypeError, "about:blank should fail");
  });

  var p2 = fetch('about:config').then(function(res) {
    ok(false, "about:config should fail");
  }, function(e) {
    ok(e instanceof TypeError, "about:config should fail");
  });

  return Promise.all([p1, p2]);
}

function testDataURL() {
  return Promise.all(
    [
      ["data:text/plain;charset=UTF-8,Hello", 'text/plain;charset=UTF-8', 'Hello'],
      ["data:text/plain;charset=utf-8;base64,SGVsbG8=", 'text/plain;charset=utf-8', 'Hello'],
      ['data:text/xml,%3Cres%3Ehello%3C/res%3E%0A', 'text/xml', '<res>hello</res>\n'],
      ['data:text/plain,hello%20pass%0A', 'text/plain', 'hello pass\n'],
      ['data:,foo', 'text/plain;charset=US-ASCII', 'foo'],
      ['data:text/plain;base64,Zm9v', 'text/plain', 'foo'],
      ['data:text/plain,foo#bar', 'text/plain', 'foo'],
      ['data:text/plain,foo%23bar', 'text/plain', 'foo#bar'],
    ].map(test => {
      var uri = test[0], contentType = test[1], expectedBody = test[2];
      return fetch(uri).then(res => {
        ok(true, "Data URL fetch should resolve");
        if (res.type == "error") {
          ok(false, "Data URL fetch should not fail.");
          return Promise.reject();
        }
        ok(res instanceof Response, "Fetch should resolve to a Response");
        is(res.status, 200, "Data URL status should be 200");
        is(res.statusText, "OK", "Data URL statusText should be OK");
        ok(res.headers.has("content-type"), "Headers must have Content-Type header");
        is(res.headers.get("content-type"), contentType, "Content-Type header should match specified value");
        return res.text().then(body => is(body, expectedBody, "Data URL Body should match"));
      })
    })
  );
}

function testSameOriginBlobURL() {
  var blob = new Blob(["english ", "sentence"], { type: "text/plain" });
  var url = URL.createObjectURL(blob);
  return fetch(url).then(function(res) {
    URL.revokeObjectURL(url);
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

function testNonGetBlobURL() {
  var blob = new Blob(["english ", "sentence"], { type: "text/plain" });
  var url = URL.createObjectURL(blob);
  return Promise.all(
    [
      "HEAD",
      "POST",
      "PUT",
      "DELETE"
    ].map(method => {
      var req = new Request(url, { method: method });
      return fetch(req).then(function(res) {
        ok(false, "Blob URL with non-GET request should not succeed");
      }).catch(function(e) {
        ok(e instanceof TypeError, "Blob URL with non-GET request should get a TypeError");
      });
    })
  ).then(function() {
    URL.revokeObjectURL(url);
  });
}

function testMozErrors() {
  // mozErrors shouldn't be available to content and be ignored.
  return fetch("http://localhost:4/should/fail", { mozErrors: true }).then(res => {
    ok(false, "Request should not succeed");
  }).catch(err => {
    ok(err instanceof TypeError);
  });
}

function testRequestMozErrors() {
  // mozErrors shouldn't be available to content and be ignored.
  const r = new Request("http://localhost:4/should/fail", { mozErrors: true });
  return fetch(r).then(res => {
    ok(false, "Request should not succeed");
  }).catch(err => {
    ok(err instanceof TypeError);
  });
}

function testViewSourceURL() {
  var p2 = fetch('view-source:/').then(function(res) {
    ok(false, "view-source: URL should fail");
  }, function(e) {
    ok(e instanceof TypeError, "view-source: URL should fail");
  });
}

function runTest() {
  return Promise.resolve()
    .then(testAboutURL)
    .then(testDataURL)
    .then(testSameOriginBlobURL)
    .then(testNonGetBlobURL)
    .then(testMozErrors)
    .then(testRequestMozErrors)
    .then(testViewSourceURL)
    // Put more promise based tests here.
}
