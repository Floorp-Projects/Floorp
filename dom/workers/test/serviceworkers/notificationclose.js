// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/
//
onnotificationclose = function(e) {
  e.waitUntil(async function() {
    let windowOpened = true;
    await clients.openWindow("hello.html")
      .catch(err => {
        windowOpened = false;
      });

    self.clients.matchAll().then(function(clients) {
      if (clients.length === 0) {
        dump("*** CLIENTS LIST EMPTY! Test will timeout! ***\n");
        return;
      }

      clients.forEach(function(client) {
        client.postMessage({ result: e.notification.data &&
                                     e.notification.data['complex'] &&
                                     e.notification.data['complex'][0] == "jsval" &&
                                     e.notification.data['complex'][1] == 5,
                             windowOpened: windowOpened});

      });
    });
  }());
}
