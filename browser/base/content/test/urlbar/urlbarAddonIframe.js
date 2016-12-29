// Listen for messages from the test.
addEventListener("TestEvent", event => {
  let type = event.detail.type;
  dump("urlbarAddonIframe.js got TestEvent, type=" + type +
       " messageID=" + event.detail.messageID + "\n");
  switch (type) {
  case "function":
    callUrlbarFunction(event.detail);
    break;
  case "event":
    expectEvent(event.detail);
    break;
  }
});

// Calls a urlbar API function.
function callUrlbarFunction(detail) {
  let args = detail.data;
  let methodName = args.shift();
  dump("urlbarAddonIframe.js calling urlbar." + methodName + "\n");
  let rv = urlbar[methodName](...args);
  ack(detail, rv);
}

// Waits for an event of a specified type to happen.
function expectEvent(detail) {
  let type = detail.data;
  dump("urlbarAddonIframe.js expecting event of type " + type + "\n");
  // Ack that the message was received and an event listener was added.
  ack(detail, null, 0);
  addEventListener(type, function onEvent(event) {
    dump("urlbarAddonIframe.js got event of type " + type + "\n");
    if (event.type != type) {
      return;
    }
    dump("urlbarAddonIframe.js got expected event\n");
    removeEventListener(type, onEvent);
    // Ack that the event was received.
    ack(detail, event.detail, 1);
  });
}

// Sends an ack to the test.
function ack(originalEventDetail, ackData = null, ackIndex = 0) {
  dispatchEvent(new CustomEvent("TestEventAck", {
    detail: {
      messageID: originalEventDetail.messageID,
      ackIndex: ackIndex,
      data: ackData,
    },
  }));
}
