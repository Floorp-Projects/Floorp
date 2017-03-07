onmessage = function(e) {
  self.clients.matchAll().then(function(res) {
    if (!res.length) {
      dump("Error: no clients are currently controlled.\n");
      return;
    }
    res[0].postMessage(indexedDB ? { available: true } :
                                   { available: false });
  });
};
