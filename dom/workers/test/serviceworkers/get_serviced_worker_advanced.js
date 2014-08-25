onmessage = function(e) {
  if (!e.data) {
    dump("ERROR: message has no data.\n");
  }

  self.clients.getServiced().then(function(res) {
    if (res.length === 0) {
      dump("ERROR: no client is currently being controlled.\n");
    }
    res[res.length - 1].postMessage(res.length);
  });
};
