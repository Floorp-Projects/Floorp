onfetch = function(e) {
  if (e.request.url.indexOf("intercept-this") != -1) {
    e.respondWith(new Response("intercepted"));
  }
}
