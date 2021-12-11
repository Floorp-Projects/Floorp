onfetch = function(e) {
  if (e.request.url.includes("intercept-this")) {
    e.respondWith(new Response("intercepted"));
  }
};
