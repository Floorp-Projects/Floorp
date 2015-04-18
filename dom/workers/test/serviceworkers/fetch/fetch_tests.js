function fetchXHR(name, onload, onerror, headers) {
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
  x.open('GET', name, true);
  x.onload = function() { onload(x) };
  x.onerror = function() { onerror(x) };
  headers = headers || [];
  headers.forEach(function(header) {
    x.setRequestHeader(header[0], header[1]);
  });
  x.send();
}

fetchXHR('synthesized.txt', function(xhr) {
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

fetch('synthesized-redirect-real-file.txt', function(xhr) {
dump("Got status AARRGH " + xhr.status + " " + xhr.responseText + "\n");
  my_ok(xhr.status == 200, "load should be successful");
  my_ok(xhr.responseText == "This is a real file.\n", "Redirect to real file should complete.");
  finish();
});

fetch('synthesized-redirect-synthesized.txt', function(xhr) {
  my_ok(xhr.status == 200, "load should be successful");
  my_ok(xhr.responseText == "synthesized response body", "load should have synthesized response");
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
  my_ok(xhr.status == 200, "gzip load should be successful");
  my_ok(xhr.responseText == expectedUncompressedResponse, "gzip load should have synthesized response.");
  my_ok(xhr.getResponseHeader("Content-Encoding") == "gzip", "Content-Encoding should be gzip.");
  my_ok(xhr.getResponseHeader("Content-Length") == "35", "Content-Length should be of original gzipped file.");
  finish();
});

fetchXHR('http://example.com/tests/dom/base/test/file_CrossSiteXHR_server.sjs?status=200&allowOrigin=*', function(xhr) {
  my_ok(xhr.status == 200, "cross origin load with correct headers should be successful");
  my_ok(xhr.getResponseHeader("access-control-allow-origin") == null, "cors headers should be filtered out");
  finish();
});

expectAsyncResult();
fetch('http://example.com/tests/dom/base/test/file_CrossSiteXHR_server.sjs?status=200&allowOrigin=*')
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
fetch('http://example.com/tests/dom/base/test/file_CrossSiteXHR_server.sjs?status=200', { mode: 'no-cors' })
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
