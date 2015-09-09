function fetchXHRWithMethod(name, method, onload, onerror, headers) {
  expectAsyncResult();

  onload = onload || function() {
    my_ok(false, "XHR load should not complete successfully");
    finish();
  };
  onerror = onerror || function() {
    my_ok(false, "XHR load for " + name + " should be intercepted successfully");
    finish();
  };

  var x = new XMLHttpRequest();
  x.open(method, name, true);
  x.onload = function() { onload(x) };
  x.onerror = function() { onerror(x) };
  headers = headers || [];
  headers.forEach(function(header) {
    x.setRequestHeader(header[0], header[1]);
  });
  x.send();
}

function fetchXHR(name, onload, onerror, headers) {
  return fetchXHRWithMethod(name, 'GET', onload, onerror, headers);
}

fetchXHR('bare-synthesized.txt', function(xhr) {
  my_ok(xhr.status == 200, "load should be successful");
  my_ok(xhr.responseText == "synthesized response body", "load should have synthesized response");
  finish();
});

fetchXHR('test-respondwith-response.txt', function(xhr) {
  my_ok(xhr.status == 200, "test-respondwith-response load should be successful");
  my_ok(xhr.responseText == "test-respondwith-response response body", "load should have response");
  finish();
});

fetchXHR('synthesized-404.txt', function(xhr) {
  my_ok(xhr.status == 404, "load should 404");
  my_ok(xhr.responseText == "synthesized response body", "404 load should have synthesized response");
  finish();
});

fetchXHR('synthesized-headers.txt', function(xhr) {
  my_ok(xhr.status == 200, "load should be successful");
  my_ok(xhr.getResponseHeader("X-Custom-Greeting") === "Hello", "custom header should be set");
  my_ok(xhr.responseText == "synthesized response body", "custom header load should have synthesized response");
  finish();
});

fetchXHR('synthesized-redirect-real-file.txt', function(xhr) {
dump("Got status AARRGH " + xhr.status + " " + xhr.responseText + "\n");
  my_ok(xhr.status == 200, "load should be successful");
  my_ok(xhr.responseText == "This is a real file.\n", "Redirect to real file should complete.");
  finish();
});

fetchXHR('synthesized-redirect-twice-real-file.txt', function(xhr) {
  my_ok(xhr.status == 200, "load should be successful");
  my_ok(xhr.responseText == "This is a real file.\n", "Redirect to real file (twice) should complete.");
  finish();
});

fetchXHR('synthesized-redirect-synthesized.txt', function(xhr) {
  my_ok(xhr.status == 200, "synth+redirect+synth load should be successful");
  my_ok(xhr.responseText == "synthesized response body", "load should have redirected+synthesized response");
  finish();
});

fetchXHR('synthesized-redirect-twice-synthesized.txt', function(xhr) {
  my_ok(xhr.status == 200, "synth+redirect+synth (twice) load should be successful");
  my_ok(xhr.responseText == "synthesized response body", "load should have redirected+synthesized (twice) response");
  finish();
});

fetchXHR('redirect.sjs', function(xhr) {
  my_ok(xhr.status == 404, "redirected load should be uninterrupted");
  finish();
});

fetchXHR('ignored.txt', function(xhr) {
  my_ok(xhr.status == 404, "load should be uninterrupted");
  finish();
});

fetchXHR('rejected.txt', null, function(xhr) {
  my_ok(xhr.status == 0, "load should not complete");
  finish();
});

fetchXHR('nonresponse.txt', null, function(xhr) {
  my_ok(xhr.status == 0, "load should not complete");
  finish();
});

fetchXHR('nonresponse2.txt', null, function(xhr) {
  my_ok(xhr.status == 0, "load should not complete");
  finish();
});

fetchXHR('nonpromise.txt', null, function(xhr) {
  my_ok(xhr.status == 0, "load should not complete");
  finish();
});

fetchXHR('headers.txt', function(xhr) {
  my_ok(xhr.status == 200, "load should be successful");
  my_ok(xhr.responseText == "1", "request header checks should have passed");
  finish();
}, null, [["X-Test1", "header1"], ["X-Test2", "header2"]]);

var expectedUncompressedResponse = "";
for (var i = 0; i < 10; ++i) {
  expectedUncompressedResponse += "hello";
}
expectedUncompressedResponse += "\n";

// ServiceWorker does not intercept, at which point the network request should
// be correctly decoded.
fetchXHR('deliver-gzip.sjs', function(xhr) {
  my_ok(xhr.status == 200, "network gzip load should be successful");
  my_ok(xhr.responseText == expectedUncompressedResponse, "network gzip load should have synthesized response.");
  my_ok(xhr.getResponseHeader("Content-Encoding") == "gzip", "network Content-Encoding should be gzip.");
  my_ok(xhr.getResponseHeader("Content-Length") == "35", "network Content-Length should be of original gzipped file.");
  finish();
});

fetchXHR('hello.gz', function(xhr) {
  my_ok(xhr.status == 200, "gzip load should be successful");
  my_ok(xhr.responseText == expectedUncompressedResponse, "gzip load should have synthesized response.");
  my_ok(xhr.getResponseHeader("Content-Encoding") == "gzip", "Content-Encoding should be gzip.");
  my_ok(xhr.getResponseHeader("Content-Length") == "35", "Content-Length should be of original gzipped file.");
  finish();
});

fetchXHR('hello-after-extracting.gz', function(xhr) {
  my_ok(xhr.status == 200, "gzip load after extracting should be successful");
  my_ok(xhr.responseText == expectedUncompressedResponse, "gzip load after extracting should have synthesized response.");
  my_ok(xhr.getResponseHeader("Content-Encoding") == "gzip", "Content-Encoding after extracting should be gzip.");
  my_ok(xhr.getResponseHeader("Content-Length") == "35", "Content-Length after extracting should be of original gzipped file.");
  finish();
});

fetchXHR('http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200&allowOrigin=*', function(xhr) {
  my_ok(xhr.status == 200, "cross origin load with correct headers should be successful");
  my_ok(xhr.getResponseHeader("access-control-allow-origin") == null, "cors headers should be filtered out");
  finish();
});

// Test that CORS preflight requests cannot be intercepted. Performs a
// cross-origin XHR that the SW chooses not to intercept. This requires a
// preflight request, which the SW must not be allowed to intercept.
fetchXHR('http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200&allowOrigin=*', null, function(xhr) {
  my_ok(xhr.status == 0, "cross origin load with incorrect headers should be a failure");
  finish();
}, [["X-Unsafe", "unsafe"]]);

// Test that CORS preflight requests cannot be intercepted. Performs a
// cross-origin XHR that the SW chooses to intercept and respond with a
// cross-origin fetch. This requires a preflight request, which the SW must not
// be allowed to intercept.
fetchXHR('http://example.org/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200&allowOrigin=*', null, function(xhr) {
  my_ok(xhr.status == 0, "cross origin load with incorrect headers should be a failure");
  finish();
}, [["X-Unsafe", "unsafe"]]);

// Test that when the page fetches a url the controlling SW forces a redirect to
// another location. This other location fetch should also be intercepted by
// the SW.
fetchXHR('something.txt', function(xhr) {
  my_ok(xhr.status == 200, "load should be successful");
  my_ok(xhr.responseText == "something else response body", "load should have something else");
  finish();
});

// Test fetch will internally get it's SkipServiceWorker flag set. The request is
// made from the SW through fetch(). fetch() fetches a server-side JavaScript
// file that force a redirect. The redirect location fetch does not go through
// the SW.
fetchXHR('redirect_serviceworker.sjs', function(xhr) {
  my_ok(xhr.status == 200, "load should be successful");
  my_ok(xhr.responseText == "// empty worker, always succeed!\n", "load should have redirection content");
  finish();
});

fetchXHR('empty-header', function(xhr) {
  my_ok(xhr.status == 200, "load should be successful");
  my_ok(xhr.responseText == "emptyheader", "load should have the expected content");
  finish();
}, null, [["emptyheader", ""]]);

expectAsyncResult();
fetch('http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200&allowOrigin=*')
.then(function(res) {
  my_ok(res.ok, "Valid CORS request should receive valid response");
  my_ok(res.type == "cors", "Response type should be CORS");
  res.text().then(function(body) {
    my_ok(body === "<res>hello pass</res>\n", "cors response body should match");
    finish();
  });
}, function(e) {
  my_ok(false, "CORS Fetch failed");
  finish();
});

expectAsyncResult();
fetch('http://example.com/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?status=200', { mode: 'no-cors' })
.then(function(res) {
  my_ok(res.type == "opaque", "Response type should be opaque");
  my_ok(res.status == 0, "Status should be 0");
  res.text().then(function(body) {
    my_ok(body === "", "opaque response body should be empty");
    finish();
  });
}, function(e) {
  my_ok(false, "no-cors Fetch failed");
  finish();
});

expectAsyncResult();
fetch('opaque-on-same-origin')
.then(function(res) {
  my_ok(false, "intercepted opaque response for non no-cors request should fail.");
  finish();
}, function(e) {
  my_ok(true, "intercepted opaque response for non no-cors request should fail.");
  finish();
});

expectAsyncResult();
fetch('http://example.com/opaque-no-cors', { mode: "no-cors" })
.then(function(res) {
  my_ok(res.type == "opaque", "intercepted opaque response for no-cors request should have type opaque.");
  finish();
}, function(e) {
  my_ok(false, "intercepted opaque response for no-cors request should pass.");
  finish();
});

expectAsyncResult();
fetch('http://example.com/cors-for-no-cors', { mode: "no-cors" })
.then(function(res) {
  my_ok(res.type == "opaque", "intercepted non-opaque response for no-cors request should resolve to opaque response.");
  finish();
}, function(e) {
  my_ok(false, "intercepted non-opaque response for no-cors request should resolve to opaque response. It should not fail.");
  finish();
});

function arrayBufferFromString(str) {
  var arr = new Uint8Array(str.length);
  for (var i = 0; i < str.length; ++i) {
    arr[i] = str.charCodeAt(i);
  }
  return arr;
}

expectAsyncResult();
fetch(new Request('body-simple', {method: 'POST', body: 'my body'}))
.then(function(res) {
  return res.text();
}).then(function(body) {
  my_ok(body == 'my bodymy body', "the body of the intercepted fetch should be visible in the SW");
  finish();
});

expectAsyncResult();
fetch(new Request('body-arraybufferview', {method: 'POST', body: arrayBufferFromString('my body')}))
.then(function(res) {
  return res.text();
}).then(function(body) {
  my_ok(body == 'my bodymy body', "the ArrayBufferView body of the intercepted fetch should be visible in the SW");
  finish();
});

expectAsyncResult();
fetch(new Request('body-arraybuffer', {method: 'POST', body: arrayBufferFromString('my body').buffer}))
.then(function(res) {
  return res.text();
}).then(function(body) {
  my_ok(body == 'my bodymy body', "the ArrayBuffer body of the intercepted fetch should be visible in the SW");
  finish();
});

expectAsyncResult();
var usp = new URLSearchParams();
usp.set("foo", "bar");
usp.set("baz", "qux");
fetch(new Request('body-urlsearchparams', {method: 'POST', body: usp}))
.then(function(res) {
  return res.text();
}).then(function(body) {
  my_ok(body == 'foo=bar&baz=quxfoo=bar&baz=qux', "the URLSearchParams body of the intercepted fetch should be visible in the SW");
  finish();
});

expectAsyncResult();
var fd = new FormData();
fd.set("foo", "bar");
fd.set("baz", "qux");
fetch(new Request('body-formdata', {method: 'POST', body: fd}))
.then(function(res) {
  return res.text();
}).then(function(body) {
  my_ok(body.indexOf("Content-Disposition: form-data; name=\"foo\"\r\n\r\nbar") <
        body.indexOf("Content-Disposition: form-data; name=\"baz\"\r\n\r\nqux"),
        "the FormData body of the intercepted fetch should be visible in the SW");
  finish();
});

expectAsyncResult();
fetch(new Request('body-blob', {method: 'POST', body: new Blob(new String('my body'))}))
.then(function(res) {
  return res.text();
}).then(function(body) {
  my_ok(body == 'my bodymy body', "the Blob body of the intercepted fetch should be visible in the SW");
  finish();
});

['DELETE', 'GET', 'HEAD', 'OPTIONS', 'POST', 'PUT'].forEach(function(method) {
  fetchXHRWithMethod('xhr-method-test.txt', method, function(xhr) {
    my_ok(xhr.status == 200, method + " load should be successful");
    my_ok(xhr.responseText == ("intercepted " + method), method + " load should have synthesized response");
    finish();
  });
});

expectAsyncResult();
fetch(new Request('empty-header', {headers:{"emptyheader":""}}))
.then(function(res) {
  return res.text();
}).then(function(body) {
  my_ok(body == "emptyheader", "The empty header was observed in the fetch event");
  finish();
});
