var requestURL = "//mochi.test:8888/tests/dom/cache/test/mochitest/mirror.sjs?" + context;
var response;
var c;
var responseText;
var name = "match-mirror" + context;

function checkResponse(r) {
  ok(r !== response, "The objects should not be the same");
  is(r.url, response.url.replace("#fragment", ""),
     "The URLs should be the same");
  is(r.status, response.status, "The status codes should be the same");
  is(r.type, response.type, "The response types should be the same");
  is(r.ok, response.ok, "Both responses should have succeeded");
  is(r.statusText, response.statusText,
     "Both responses should have the same status text");
  is(r.headers.get("Mirrored"), response.headers.get("Mirrored"),
     "Both responses should have the same Mirrored header");
  return r.text().then(function(text) {
    is(text, responseText, "The response body should be correct");
  });
}

fetch(new Request(requestURL, {headers: {"Mirror": "bar"}})).then(function(r) {
  is(r.headers.get("Mirrored"), "bar", "The server should give back the correct header");
  response = r;
  return response.text();
}).then(function(text) {
  responseText = text;
  return caches.open(name);
}).then(function(cache) {
  c = cache;
  return c.add(new Request(requestURL, {headers: {"Mirror": "foo"}}));
}).then(function() {
  // Overwrite the request, to replace the entry stored in response_headers
  // with a different value.
  return c.add(new Request(requestURL, {headers: {"Mirror": "bar"}}));
}).then(function() {
  return c.matchAll();
}).then(function(r) {
  is(r.length, 1, "Only one request should be in the cache");
  return checkResponse(r[0]);
}).then(function() {
  return caches.delete(name);
}).then(function(deleted) {
  ok(deleted, "The cache should be deleted successfully");
  testDone();
});
