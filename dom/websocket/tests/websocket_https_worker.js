onmessage = function () {
  var wsCreated = true;
  try {
    new WebSocket(
      "ws://mochi.test:8888/tests/dom/websocket/tests/file_websocket_hello"
    );
  } catch (e) {
    wsCreated = false;
  }
  postMessage(wsCreated ? "created" : "not created");
};
