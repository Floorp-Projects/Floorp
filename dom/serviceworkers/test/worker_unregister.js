onmessage = function(e) {
  clients.matchAll().then(function(c) {
    if (c.length === 0) {
      // We cannot proceed.
      return;
    }

    registration.unregister().then(function() {
      c[0].postMessage('DONE');
    }, function() {
      c[0].postMessage('ERROR');
    }).then(function() {
      c[0].postMessage('FINISH');
    });
  });
}
