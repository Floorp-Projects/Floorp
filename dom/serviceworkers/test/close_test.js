function ok(v, msg) {
  client.postMessage({status: "ok", result: !!v, message: msg});
}

var client;
onmessage = function(e) {
  if (e.data.message == "start") {
    self.clients.matchAll().then(function(clients) {
      client = clients[0];
      try {
        close();
        ok(false, "close() should throw");
      } catch (ex) {
        ok(ex.name === "InvalidAccessError", "close() should throw InvalidAccessError");
      }
      client.postMessage({status: "done"});
    });
  }
}
