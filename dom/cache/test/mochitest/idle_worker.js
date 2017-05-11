// Touch CacheStorage, but then idle and do nothing.
const name = 'idle_worker_cache';
var cache;
self.caches.open(name).then(c => {
  cache = c;
  return self.caches.delete(name);
}).then(_ => {
  postMessage('LOADED');
});
