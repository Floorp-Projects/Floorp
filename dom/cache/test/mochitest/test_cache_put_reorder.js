var name = "putreorder" + context;
var c;

var reqs = [
  "//mochi.test:8888/?foo" + context,
  "//mochi.test:8888/?bar" + context,
  "//mochi.test:8888/?baz" + context,
];

caches.open(name).then(function(cache) {
  c = cache;
  return c.addAll(reqs);
}).then(function() {
  return c.put(reqs[1], new Response("overwritten"));
}).then(function() {
  return c.keys();
}).then(function(keys) {
  is(keys.length, 3, "Correct number of entries expected");
  ok(keys[0].url.indexOf(reqs[0]) >= 0, "The first entry should be untouched");
  ok(keys[2].url.indexOf(reqs[1]) >= 0, "The second entry should be moved to the end");
  ok(keys[1].url.indexOf(reqs[2]) >= 0, "The third entry should now be the second one");
  return c.match(reqs[1]);
}).then(function(r) {
  return r.text();
}).then(function(body) {
  is(body, "overwritten", "The body should be overwritten");
  return caches.delete(name);
}).then(function(deleted) {
  ok(deleted, "The cache should be deleted successfully");
  testDone();
});
