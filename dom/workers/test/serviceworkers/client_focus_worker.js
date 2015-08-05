onmessage = function(e) {
  if (!e.source) {
    dump("ERROR: message doesn't have a source.");
  }

  // The client should be a window client
  if (e.source instanceof WindowClient) {
    // this will dispatch a focus event on the client
    e.source.focus().then(function(client) {
      client.postMessage(client.focused);
    });
  } else {
    dump("ERROR: client should be a WindowClient");
  }
};
