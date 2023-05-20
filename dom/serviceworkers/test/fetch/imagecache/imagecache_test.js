function synthesizeImage() {
  return clients.matchAll().then(clients => {
    var url = "image-40px.png";
    clients.forEach(client => {
      client.postMessage(url);
    });
    return fetch(url);
  });
}

self.addEventListener("fetch", function (event) {
  if (event.request.url.includes("image-20px.png")) {
    event.respondWith(synthesizeImage());
  }
});
