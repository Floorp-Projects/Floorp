// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
//

function promisifyTimerFocus(client, delay) {
  return new Promise(function(resolve, reject) {
    setTimeout(function() {
      client.focus().then(resolve, reject);
    }, delay);
  });
}

onnotificationclick = function(e) {
  e.waitUntil(self.clients.matchAll().then(function(clients) {
    if (clients.length === 0) {
      dump("********************* CLIENTS LIST EMPTY! Test will timeout! ***********************\n");
      return Promise.resolve();
    }

    var immediatePromise = clients[0].focus();
    var withinTimeout = promisifyTimerFocus(clients[0], 100);

    var afterTimeout = promisifyTimerFocus(clients[0], 2000).then(function() {
      throw "Should have failed!";
    }, function() {
      return Promise.resolve();
    });

    return Promise.all([immediatePromise, withinTimeout, afterTimeout]).then(function() {
      clients.forEach(function(client) {
        client.postMessage({ok: true});
      });
    }).catch(function(e) {
      dump("Error " + e + "\n");
      clients.forEach(function(client) {
        client.postMessage({ok: false});
      });
    });
  }));
}
