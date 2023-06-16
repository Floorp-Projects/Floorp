self.addEventListener("fetch", function (event) {
  if (event.request.url.includes("synth.html")) {
    var body =
      "<script>" +
      'window.parent.postMessage({status: "done", cookie: document.cookie}, "*");' +
      "</script>";
    event.respondWith(
      new Response(body, { headers: { "Content-Type": "text/html" } })
    );
  }
});
