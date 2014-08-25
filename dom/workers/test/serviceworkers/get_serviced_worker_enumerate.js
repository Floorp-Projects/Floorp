onmessage = function() {
  self.clients.getServiced().then(function(result) {
    for (i = 0; i < result.length; i++) {
      result[i].postMessage(i);
    }
  });
};
