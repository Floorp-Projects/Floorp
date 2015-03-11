function OnMessage(e)
{
  if (e.data.msg == "whoareyou") {
    if ("ServiceWorker" in self) {
      self.clients.matchAll().then(function(clients) {
        clients[0].postMessage({result: "serviceworker"});
      });
    } else {
      port.postMessage({result: "sharedworker"});
    }
  }
};

var port;
onconnect = function(e) {
  port = e.ports[0];
  port.onmessage = OnMessage;
  port.start();
};

onmessage = OnMessage;
