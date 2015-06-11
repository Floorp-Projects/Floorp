self.addEventListener("fetch", function(event) {
  dump("fetch " + event.request.url + "\n");
  if (event.request.url.indexOf("iframe2.html") >= 0) {
    var body =
      "<script>" +
        "window.parent.postMessage({" +
          "source: 'iframe', status: 'swresponse'" +
        "}, '*');" +
      "</script>";
    event.respondWith(new Response(body, {
      headers: {'Content-Type': 'text/html'}
    }));
  }
});
