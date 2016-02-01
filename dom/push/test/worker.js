// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

this.onpush = handlePush;
this.onmessage = handleMessage;

function getJSON(data) {
  var result = {
    ok: false,
  };
  try {
    result.value = data.json();
    result.ok = true;
  } catch (e) {
    // Ignore syntax errors for invalid JSON.
  }
  return result;
}

function handlePush(event) {

  event.waitUntil(self.clients.matchAll().then(function(result) {
    if (event instanceof PushEvent) {
      if (!('data' in event)) {
        result[0].postMessage({type: "finished", okay: "yes"});
        return;
      }
      var message = {
        type: "finished",
        okay: "yes",
      };
      if (event.data) {
        message.data = {
          text: event.data.text(),
          arrayBuffer: event.data.arrayBuffer(),
          json: getJSON(event.data),
          blob: event.data.blob(),
        };
      }
      result[0].postMessage(message);
      return;
    }
    result[0].postMessage({type: "finished", okay: "no"});
  }));
}

function handleMessage(event) {
  if (event.data.type == "publicKey") {
    event.waitUntil(self.registration.pushManager.getSubscription().then(subscription => {
      event.ports[0].postMessage({
        p256dh: subscription.getKey("p256dh"),
        auth: subscription.getKey("auth"),
      });
    }).catch(error => {
      event.ports[0].postMessage({
        error: String(error),
      });
    }));
    return;
  }
  event.ports[0].postMessage({
    error: "Invalid message type: " + event.data.type,
  });
}
