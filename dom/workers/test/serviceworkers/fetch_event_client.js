var CLIENT_URL =
  "http://mochi.test:8888/tests/dom/workers/test/serviceworkers/sw_clients/dummy.html"

self.addEventListener("fetch", function(event) {
  event.client.postMessage({status: event.client.url === CLIENT_URL});
});
