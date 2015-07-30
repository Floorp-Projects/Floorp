// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

// This worker is used for two types of tests. `handlePush` sends messages to
// `frame.html`, which verifies that the worker can receive push messages.

// `handleMessage` receives a message from `test_push_manager_worker.html`, and
// verifies that `PushManager` can be used from the worker.

this.onpush = handlePush;
this.onmessage = handleMessage;

function handlePush(event) {

  self.clients.matchAll().then(function(result) {
    if (event instanceof PushEvent &&
      event.data instanceof PushMessageData &&
      event.data.text === undefined &&
      event.data.json === undefined &&
      event.data.arrayBuffer === undefined &&
      event.data.blob === undefined) {

      result[0].postMessage({type: "finished", okay: "yes"});
      return;
    }
    result[0].postMessage({type: "finished", okay: "no"});
  });
}

function handleMessage(event) {
  self.registration.pushManager.getSubscription().then(subscription => {
    if (subscription.endpoint != event.data.endpoint) {
      throw new Error("Wrong push endpoint in worker");
    }
    return subscription.unsubscribe();
  }).then(result => {
    if (!result) {
      throw new Error("Error dropping subscription in worker");
    }
    return self.registration.pushManager.getSubscription();
  }).then(subscription => {
    if (subscription) {
      throw new Error("Subscription not dropped in worker");
    }
    return self.registration.pushManager.subscribe();
  }).then(subscription => {
    event.ports[0].postMessage({endpoint: subscription.endpoint});
  }).catch(error => {
    event.ports[0].postMessage({error: error});
  });
}
