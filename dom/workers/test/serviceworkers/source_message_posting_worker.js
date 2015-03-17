onmessage = function(e) {
  if (!e.source) {
    dump("ERROR: message doesn't have a source.");
  }

  // The client should be a window client
  if (e.source instanceof  WindowClient) {
    e.source.postMessage(e.data);
  } else {
    e.source.postMessage("ERROR. source is not a window client.");
  }
};
