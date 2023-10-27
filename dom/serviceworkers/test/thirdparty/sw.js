self.addEventListener("fetch", function (event) {
  dump("fetch " + event.request.url + "\n");
  if (event.request.url.includes("iframe2.html")) {
    var body =
      "<script>" +
      "window.parent.postMessage({" +
      "source: 'iframe', status: 'swresponse'" +
      "}, '*');" +
      "var w = new Worker('worker.js');" +
      "w.onmessage = function(evt) {" +
      "window.parent.postMessage({" +
      "source: 'worker'," +
      "status: evt.data," +
      "}, '*');" +
      "};" +
      "</script>";
    event.respondWith(
      new Response(body, {
        headers: { "Content-Type": "text/html" },
      })
    );
    return;
  }
  if (event.request.url.includes("worker.js")) {
    var body = "self.postMessage('worker-swresponse');";
    event.respondWith(
      new Response(body, {
        headers: { "Content-Type": "application/javascript" },
      })
    );
  }
});
