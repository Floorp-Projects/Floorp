onmessage = function(e) {
  dump("MatchAllPropertiesWorker:" + e.data + "\n");
  self.clients.matchAll().then(function(res) {
    if (!res.length) {
      dump("ERROR: no clients are currently controlled.\n");
    }

    for (i = 0; i < res.length; i++) {
      client = res[i];
      response = {
        id: client.id,
        url: client.url,
        visibilityState: client.visibilityState,
        focused: client.focused,
        frameType: client.frameType
      };
      client.postMessage(response);
    }
  });
}
