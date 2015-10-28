function synthesizeImage(suffix) {
  // Serve image-20px for the first page, and image-40px for the second page.
  return clients.matchAll().then(clients => {
    var url = "image-20px.png";
    clients.forEach(client => {
      if (client.url.indexOf("?new") > 0) {
        url = "image-40px.png";
      }
      client.postMessage({suffix: suffix, url: url});
    });
    return fetch(url);
  }).then(response => {
    return response.arrayBuffer();
  }).then(ab => {
    var headers;
    if (suffix == "") {
      headers = {
        "Content-Type": "image/png",
        "Date": "Tue, 1 Jan 1990 01:02:03 GMT",
        "Cache-Control": "max-age=1",
      };
    } else {
      headers = {
        "Content-Type": "image/png",
        "Cache-Control": "no-cache",
      };
    }
    return new Response(ab, {
      status: 200,
      headers: headers,
    });
  });
}

self.addEventListener("fetch", function(event) {
  if (event.request.url.indexOf("image.png") >= 0) {
    event.respondWith(synthesizeImage(""));
  } else if (event.request.url.indexOf("image2.png") >= 0) {
    event.respondWith(synthesizeImage("2"));
  }
});
