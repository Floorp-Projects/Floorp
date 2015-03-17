onmessage = function(e) {
  dump("MatchAllClientIdWorker:" + e.data + "\n");
  var id = [];
  var iterations = 5;
  var counter = 0;

  for (var i = 0; i < iterations; i++) {
    self.clients.matchAll().then(function(res) {
      if (!res.length) {
        dump("ERROR: no clients are currently controlled.\n");
      }

      client = res[0];
      id[counter] = client.id;
      counter++;
      if (counter >= iterations) {
        var response = true;
        for (var index = 1; index < iterations; index++) {
          if (id[0] != id[index]) {
            response = false;
            break;
          }
        }
        client.postMessage(response);
      }
    });
  }
}
