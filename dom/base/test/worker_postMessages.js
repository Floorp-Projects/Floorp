function test_workers() {
  onmessage = function(e) {
    postMessage(e.data, e.ports);
  }
}

function test_broadcastChannel() {
  var bc = new BroadcastChannel('postMessagesTest_inWorkers');
  bc.onmessage = function(e) {
    postMessage(e.data);
  }
}

function test_messagePort(port) {
  port.onmessage = function(e) {
    postMessage(e.data, e.ports);
  }
}

onmessage = function(e) {
  if (e.data == 'workers') {
    test_workers();
    postMessage('ok');
  } else if (e.data == 'broadcastChannel') {
    test_broadcastChannel();
    postMessage('ok');
  } else if (e.data == 'messagePort') {
    test_messagePort(e.ports[0]);
    postMessage('ok');
  } else {
    postMessage('ko');
  }
}
