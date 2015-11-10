onmessage = function(e) {
  if (!e.source) {
    dump("ERROR: message doesn't have a source.");
  }

  if (!(e instanceof ExtendableMessageEvent)) {
    e.source.postMessage("ERROR. event is not an extendable message event.");
  }

  // The client should be a window client
  if (e.source instanceof  WindowClient) {
    e.source.postMessage(e.data);
  } else {
    e.source.postMessage("ERROR. source is not a window client.");
  }
};
