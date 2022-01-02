onconnect = function(evt) {
  var ws = new WebSocket(
    "ws://mochi.test:8888/tests/dom/websocket/tests/file_websocket_hello"
  );

  ws.onopen = function(e) {
    evt.ports[0].postMessage({
      type: "status",
      status: true,
      msg: "OnOpen called",
    });
    ws.send("data");
  };

  ws.onclose = function(e) {};

  ws.onerror = function(e) {
    evt.ports[0].postMessage({
      type: "status",
      status: false,
      msg: "onerror called!",
    });
  };

  ws.onmessage = function(e) {
    evt.ports[0].postMessage({
      type: "status",
      status: e.data == "Hello world!",
      msg: "Wrong data",
    });
    ws.close();
    evt.ports[0].postMessage({ type: "finish" });
  };
};
