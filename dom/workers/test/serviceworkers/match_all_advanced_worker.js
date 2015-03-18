onmessage = function(e) {
  self.clients.matchAll().then(function(clients) {
    e.source.postMessage(clients.length);
  });
}
