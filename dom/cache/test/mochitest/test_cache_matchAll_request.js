var request1 = new Request("//mochi.test:8888/?1&" + context);
var request2 = new Request("//mochi.test:8888/?2&" + context);
var request3 = new Request("//mochi.test:8888/?3&" + context);
var response1, response3;
var c;
var response1Text, response3Text;
var name = "matchAll-request" + context;

function checkResponse(r, response, responseText) {
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

fetch(new Request(request1)).then(function(r) {
  response1 = r;
  return response1.text();
}).then(function(text) {
  response1Text = text;
  return fetch(new Request(request3));
}).then(function(r) {
  response3 = r;
  return response3.text();
}).then(function(text) {
  response3Text = text;
  return caches.open(name);
}).then(function(cache) {
  c = cache;
  return c.add(request1);
}).then(function() {
  return c.add(request3);
}).then(function() {
  return c.matchAll(request1);
}).then(function(r) {
  is(r.length, 1, "Should only find 1 item");
  return checkResponse(r[0], response1, response1Text);
}).then(function() {
  return c.matchAll(request3);
}).then(function(r) {
  is(r.length, 1, "Should only find 1 item");
  return checkResponse(r[0], response3, response3Text);
}).then(function() {
  return c.matchAll();
}).then(function(r) {
  is(r.length, 2, "Should find 2 items");
  return Promise.all([
    checkResponse(r[0], response1, response1Text),
    checkResponse(r[1], response3, response3Text)
  ]);
}).then(function() {
  return c.matchAll({cacheName: name + "mambojambo"});
}).catch(function(err) {
  is(err.name, "NotFoundError", "Searching in the wrong cache should not succeed");
}).then(function() {
  return caches.delete(name);
}).then(function(success) {
  ok(success, "We should be able to delete the cache successfully");
  // Make sure that the cache is still usable after deletion.
  return c.matchAll(request1);
}).then(function(r) {
  is(r.length, 1, "Should only find 1 item");
  return checkResponse(r[0], response1, response1Text);
}).then(function() {
  return c.matchAll(request3);
}).then(function(r) {
  is(r.length, 1, "Should only find 1 item");
  return checkResponse(r[0], response3, response3Text);
}).then(function() {
  return c.matchAll();
}).then(function(r) {
  is(r.length, 2, "Should find 2 items");
  return Promise.all([
    checkResponse(r[0], response1, response1Text),
    checkResponse(r[1], response3, response3Text)
  ]);
}).then(function() {
  // Now, drop the cache, reopen and verify that we can't find the request any more.
  c = null;
  return caches.open(name);
}).then(function(cache) {
  return cache.matchAll();
}).catch(function(err) {
  is(err.name, "NotFoundError", "Searching in the cache after deletion should not succeed");
}).then(function() {
  testDone();
});
