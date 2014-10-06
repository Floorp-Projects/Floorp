onmessage = function(e) {
  clients.getServiced().then(function(c) {
    if (c.length == 0) {
      // We cannot proceed.
      return;
    }

    unregister().then(function() {
      c[0].postMessage('DONE');
    }, function() {
      c[0].postMessage('ERROR');
    }).then(function() {
      c[0].postMessage('FINISH');
    });
  });
}
