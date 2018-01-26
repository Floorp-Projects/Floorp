// Cross origin request
var prefix = 'http://example.com/tests/dom/workers/test/serviceworkers/eventsource/';

self.importScripts('eventsource_worker_helper.js');

self.addEventListener('fetch', function (event) {
  var request = event.request;
  var url = new URL(request.url);

  if (url.pathname !== '/tests/dom/workers/test/serviceworkers/eventsource/eventsource.resource') {
    return;
  }

  ok(request.mode === 'cors', 'EventSource should make a CORS request');
  ok(request.cache === 'no-store', 'EventSource should make a no-store request');
  var fetchRequest = new Request(prefix + 'eventsource.resource', { mode: 'no-cors'});
  event.respondWith(fetch(fetchRequest).then((fetchResponse) => {
    return fetchResponse;
  }));
});
