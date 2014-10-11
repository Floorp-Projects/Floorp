onmessage = function(event) {
  if (event.data != 0) {
    var worker = new Worker('websocket_basic_worker.js');
    worker.onmessage = function(event) {
      postMessage(event.data);
    }

    worker.postMessage(event.data - 1);
    return;
  }

  status = false;
  try {
    if ((WebSocket instanceof Object)) {
      status = true;
    }
  } catch(e) {
  }

  postMessage({type: 'status', status: status, msg: 'WebSocket object:' + WebSocket});

  var ws = new WebSocket("ws://mochi.test:8888/tests/content/base/test/file_websocket_hello");
  ws.onopen = function(e) {
    postMessage({type: 'status', status: true, msg: 'OnOpen called' });
    ws.send("data");
  }

  ws.onclose = function(e) {}

  ws.onerror = function(e) {
    postMessage({type: 'status', status: false, msg: 'onerror called!'});
  }

  ws.onmessage = function(e) {
    postMessage({type: 'status', status: e.data == 'Hello world!', msg: 'Wrong data'});
    ws.close();
    postMessage({type: 'finish' });
  }
}
