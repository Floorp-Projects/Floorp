addEventListener("fetch", event => {
  var url = event.request.url;
  var badURL = url.indexOf("secret.html") > -1;
  event.respondWith(
    new Promise(resolve => {
      clients.matchAll().then(clients => {
        for (var client of clients) {
          if (client.url.indexOf("index.html") > -1) {
            client.postMessage({status: "ok", result: !badURL, message: "Should not find a bad URL (" + url + ")"});
            break;
          }
        }
        resolve(fetch(event.request));
      });
    })
  );
});
