var request = new Request("//mochi.test:8888/?" + context);
var response;
var c;
var responseText;
var name = "match-request" + context;

function checkResponse(r) {
  ok(r !== response, "The objects should not be the same");
  is(r.url, response.url, "The URLs should be the same");
  is(r.status, response.status, "The status codes should be the same");
  is(r.type, response.type, "The response types should be the same");
  is(r.ok, response.ok, "Both responses should have succeeded");
  is(r.statusText, response.statusText,
     "Both responses should have the same status text");
  return r.text().then(function(text) {
    is(text, responseText, "The response body should be correct");
  });
}

fetch(new Request(request)).then(function(r) {
  response = r;
  return response.text();
}).then(function(text) {
  responseText = text;
  return caches.open(name);
}).then(function(cache) {
  c = cache;
  return c.add(request);
}).then(function() {
  return c.match(request);
}).then(function(r) {
  return checkResponse(r);
}).then(function() {
  return caches.match(request);
}).then(function(r) {
  return checkResponse(r);
}).then(function() {
  return caches.match(request, {cacheName: name});
}).then(function(r) {
  return checkResponse(r);
}).then(function() {
  return caches.match(request, {cacheName: name + "mambojambo"});
}).catch(function(err) {
  is(err.name, "NotFoundError", "Searching in the wrong cache should not succeed");
}).then(function() {
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
}).catch(function(err) {
  is(err.name, "NotFoundError", "Searching in the cache after deletion should not succeed");
}).then(function() {
  testDone();
});
