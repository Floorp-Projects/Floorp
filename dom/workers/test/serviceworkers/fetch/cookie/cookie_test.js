self.addEventListener("fetch", function(event) {
  if (event.request.url.indexOf("synth.html") >= 0) {
    var body = '<script>' +
        'window.parent.postMessage({status: "done", cookie: document.cookie}, "*");' +
      '</script>';
    event.respondWith(new Response(body, {headers: {'Content-Type': 'text/html'}}));
  }
});
