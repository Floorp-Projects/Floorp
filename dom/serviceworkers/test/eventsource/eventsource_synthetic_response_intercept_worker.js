/* eslint-disable mozilla/no-comparison-or-assignment-inside-ok */

self.importScripts("eventsource_worker_helper.js");

self.addEventListener("fetch", function (event) {
  var request = event.request;
  var url = new URL(request.url);

  if (
    url.pathname !==
    "/tests/dom/serviceworkers/test/eventsource/eventsource.resource"
  ) {
    return;
  }

  ok(request.mode === "cors", "EventSource should make a CORS request");
  var headerList = {
    "Content-Type": "text/event-stream",
    "Cache-Control": "no-cache, must-revalidate",
  };
  var headers = new Headers(headerList);
  var init = {
    headers,
    mode: "cors",
  };
  var body = "data: data0\r\r";
  var response = new Response(body, init);
  event.respondWith(response);
});
