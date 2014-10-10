onmessage = function(event) {
  if (event.data != 0) {
    var worker = new Worker('websocket_loadgroup_worker.js');
    worker.onmessage = function(event) {
      postMessage(event.data);
    }

    worker.postMessage(event.data - 1);
    return;
  }

  var ws = new WebSocket("ws://mochi.test:8888/tests/content/base/test/file_websocket_hello");
  ws.onopen = function(e) {
    postMessage('opened');
  }

  ws.onclose = function(e) {
    postMessage('closed');
  }

  ws.onerror = function(e) {
    postMessage('error');
  }
}
