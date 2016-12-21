var request = new Request("//mochi.test:8888/?" + context + "#fragment");
var requestWithAltQS = new Request("//mochi.test:8888/?queryString");
var unknownRequest = new Request("//mochi.test:8888/non/existing/path?" + context);
var response;
var c;
var responseText;
var name = "match-request" + context;

function checkResponse(r, expectedBody) {
  if (expectedBody === undefined) {
    expectedBody = responseText;
  }
  ok(r !== response, "The objects should not be the same");
  is(r.url, response.url.replace("#fragment", ""),
     "The URLs should be the same");
  is(r.status, response.status, "The status codes should be the same");
  is(r.type, response.type, "The response types should be the same");
  is(r.ok, response.ok, "Both responses should have succeeded");
  is(r.statusText, response.statusText,
     "Both responses should have the same status text");
  return r.text().then(function(text) {
    // Avoid dumping out the large response text to the log if they're equal.
    if (text !== expectedBody) {
      is(text, responseText, "The response body should be correct");
    }
  });
}
fetch(new Request(request)).then(function(r) {
  response = r;
  return response.text();
}).then(function(text) {
  responseText = text;
  return testRequest(request, unknownRequest, requestWithAltQS,
                     request.url.replace("#fragment", "#other"));
}).then(function() {
  return testRequest(request.url, unknownRequest.url, requestWithAltQS.url,
                     request.url.replace("#fragment", "#other"));
}).then(function() {
  testDone();
});
// The request argument can either be a URL string, or a Request object.
function testRequest(request, unknownRequest, requestWithAlternateQueryString,
                     requestWithDifferentFragment) {
  return caches.open(name).then(function(cache) {
    c = cache;
    return c.add(request);
  }).then(function() {
    return Promise.all(
      ["HEAD", "POST", "PUT", "DELETE", "OPTIONS"]
        .map(function(method) {
          var r = new Request(request, {method: method});
          return c.add(r)
            .then(function() {
              ok(false, "Promise should be rejected");
            }, function(err) {
              is(err.name, "TypeError", "Adding a request with type '" + method + "' should fail");
            });
        })
    );
  }).then(function() {
    return c.match(request);
  }).then(function(r) {
    return checkResponse(r);
  }).then(function() {
    return c.match(new Request(request, {method: "HEAD"}));
  }).then(function(r) {
    is(typeof r, "undefined", "Searching for an HEAD request should not succeed");
    return c.match(new Request(request, {method: "HEAD"}), {ignoreMethod: true});
  }).then(function(r) {
    return checkResponse(r);
  }).then(function() {
    return Promise.all(
      ["HEAD", "POST", "PUT", "DELETE", "OPTIONS"]
        .map(function(method) {
          var req = new Request(request, {method: method});
          return c.match(req)
            .then(function(r) {
              is(typeof r, "undefined", "Searching for a request with a non-GET method should not succeed");
              return c.match(req, {ignoreMethod: true});
            }).then(function(r) {
              return checkResponse(r);
            });
        })
    );
  }).then(function() {
    return caches.match(request);
  }).then(function(r) {
    return checkResponse(r);
  }).then(function() {
    return caches.match(requestWithDifferentFragment);
  }).then(function(r) {
    return checkResponse(r);
  }).then(function() {
    return caches.match(requestWithAlternateQueryString,
                        {ignoreSearch: true, cacheName: name});
  }).then(function(r) {
    return checkResponse(r);
  }).then(function() {
    return caches.match(request, {cacheName: name});
  }).then(function(r) {
    return checkResponse(r);
  }).then(function() {
    return caches.match(request, {cacheName: name + "mambojambo"})
      .then(function(result) {
        is(typeof r, "undefined", 'Searching in the wrong cache should resolve to undefined');
        return caches.has(name + "mambojambo");
      }).then(function(hasCache) {
        ok(!hasCache, 'The wrong cache should still not exist');
      });
  }).then(function() {
    // Make sure that cacheName is ignored on Cache
    return c.match(request, {cacheName: name + "mambojambo"});
  }).then(function(r) {
    return checkResponse(r);
  }).then(function() {
    return c.match(unknownRequest);
  }).then(function(r) {
    is(typeof r, "undefined", "Searching for an unknown request should not succeed");
    return caches.match(unknownRequest);
  }).then(function(r) {
    is(typeof r, "undefined", "Searching for an unknown request should not succeed");
    return caches.match(unknownRequest, {cacheName: name});
  }).then(function(r) {
    is(typeof r, "undefined", "Searching for an unknown request should not succeed");
    return caches.delete(name);
  }).then(function(success) {
    ok(success, "We should be able to delete the cache successfully");
    // Make sure that the cache is still usable after deletion.
    return c.match(request);
  }).then(function(r) {
    return checkResponse(r);
  }).then(function() {
    // Now, drop the cache, reopen and verify that we can't find the request any more.
    c = null;
    return caches.open(name);
  }).then(function(cache) {
    return cache.match(request);
  }).then(function(r) {
    is(typeof r, "undefined", "Searching in the cache after deletion should not succeed");
    return caches.delete(name);
  }).then(function(deleted) {
    ok(deleted, "The cache should be deleted successfully");
  });
}
