onmessage = function() {
  var wsCreated = true;
  try {
    new WebSocket("ws://mochi.test:8888/tests/dom/base/test/file_websocket_hello");
  } catch(e) {
    wsCreated = false;
  }
  postMessage(wsCreated ? "created" : "not created");
};
