function test_workers() {
  onmessage = function (e) {
    postMessage(e.data, e.ports);
  };

  onmessageerror = function (e) {
    postMessage("error");
  };
}

function test_sharedWorkers(port) {
  port.onmessage = function (e) {
    if (e.data == "terminate") {
      close();
    } else {
      port.postMessage(e.data, e.ports);
    }
  };

  port.onmessageerror = function (e) {
    port.postMessage("error");
  };
}

function test_broadcastChannel(obj) {
  var bc = new BroadcastChannel("postMessagesTest_inWorkers");
  bc.onmessage = function (e) {
    obj.postMessage(e.data);
  };

  bc.onmessageerror = function () {
    obj.postMessage("error");
  };
}

function test_messagePort(port) {
  port.onmessage = function (e) {
    postMessage(e.data, e.ports);
  };

  port.onmessageerror = function (e) {
    postMessage("error");
  };
}

onconnect = function (e) {
  e.ports[0].onmessage = ee => {
    if (ee.data == "sharedworkers") {
      test_sharedWorkers(e.ports[0]);
      e.ports[0].postMessage("ok");
    } else if (ee.data == "broadcastChannel") {
      test_broadcastChannel(e.ports[0]);
      e.ports[0].postMessage("ok");
    } else if (ee.data == "terminate") {
      close();
    }
  };
};

onmessage = function (e) {
  if (e.data == "workers") {
    test_workers();
    postMessage("ok");
  } else if (e.data == "broadcastChannel") {
    test_broadcastChannel(self);
    postMessage("ok");
  } else if (e.data == "messagePort") {
    test_messagePort(e.ports[0]);
    postMessage("ok");
  } else {
    postMessage("ko");
  }
};
