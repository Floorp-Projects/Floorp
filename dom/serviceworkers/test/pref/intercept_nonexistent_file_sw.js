onfetch = function (e) {
  if (e.request.url.match(/this_file_does_not_exist.txt$/)) {
    e.respondWith(new Response("intercepted"));
  }
};
