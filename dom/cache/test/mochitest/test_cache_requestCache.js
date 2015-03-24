var name = "requestCache" + context;
var c;

var reqWithoutCache = new Request("//mochi.test:8888/?noCache" + context);
var reqWithCache = new Request("//mochi.test:8888/?withCache" + context,
                               {cache: "force-cache"});

// Sanity check
is(reqWithoutCache.cache, "default", "Correct default value");
is(reqWithCache.cache, "force-cache", "Correct value set by the ctor");

caches.open(name).then(function(cache) {
  c = cache;
  return c.addAll([reqWithoutCache, reqWithCache]);
}).then(function() {
  return c.keys();
}).then(function(keys) {
  is(keys.length, 2, "Correct number of requests");
  is(keys[0].url, reqWithoutCache.url, "Correct URL");
  is(keys[0].cache, reqWithoutCache.cache, "Correct cache attribute");
  is(keys[1].url, reqWithCache.url, "Correct URL");
  is(keys[1].cache, reqWithCache.cache, "Correct cache attribute");
  return caches.delete(name);
}).then(function(deleted) {
  ok(deleted, "The cache should be successfully deleted");
  testDone();
});
