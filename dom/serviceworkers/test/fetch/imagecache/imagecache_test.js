function synthesizeImage() {
  return clients.matchAll().then(clients => {
    var url = "image-40px.png";
    clients.forEach(client => {
      client.postMessage(url);
    });
    return fetch(url);
  });
}

self.addEventListener("fetch", function(event) {
  if (event.request.url.indexOf("image-20px.png") >= 0) {
    event.respondWith(synthesizeImage());
  }
});
