var clients = new Array();
clients.length = 0;

var broadcast = function(message) {
  var length = clients.length;
  for (var i = 0; i < length; i++) {
    port = clients[i];
    port.postMessage(message);
  }
}

onconnect = function(e) {
  clients.push(e.ports[0]);
  if (clients.length == 1) {
    clients[0].postMessage("Connected");
  } else if (clients.length == 2) {
    broadcast("BothConnected");
    clients[0].onmessage = function(e) {
      if (e.data == "StartFetchWithWrongIntegrity") {
        // The fetch will succeed because the integrity value is invalid and we
        // are looking for the console message regarding the bad integrity value.
        fetch("SharedWorker_SRIFailed.html", {"integrity": "abc"}).then(
            function () {
                clients[0].postMessage('SRI_failed');
            });
      }
    }
  }
}
