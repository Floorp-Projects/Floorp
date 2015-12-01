function ok(aCondition, aMessage) {
  if (aCondition) {
    alert("OK: " + aMessage);
  } else {
    alert("KO: " + aMessage);
  }
}

function ready() {
  alert("READY");
}

let _port = null;
let responseReceived = false;

function onmessage(message) {
  responseReceived = (message.data == "response");
  ok(responseReceived, "response received");
}

function onclose() {
  ok(true, "onclose received");
  if (responseReceived) {
    ready();
  }
}

(function makeConnection() {
  ok(true, "Connecting");
  navigator.mozApps.getSelf().onsuccess = event => {
    ok(true, "Got self");
    let app = event.target.result;
    app.connect("a-connection").then(ports => {
      if (!ports || !ports.length) {
        return ok(false, "No ports");
      }
      ok(true, "Got port");
      _port = ports[0];
      _port.onmessage = onmessage;
      _port.onclose = onclose;
      _port.postMessage('something');
    }).catch(error => {
      ok(false, "Unexpected " + error);
    });
  };
})();
