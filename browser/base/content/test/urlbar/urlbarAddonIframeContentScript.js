// Forward messages from the test to the iframe as events.
addMessageListener("TestMessage", msg => {
  content.dispatchEvent(new content.CustomEvent("TestEvent", {
    detail: Components.utils.cloneInto(msg.data, content),
  }));
});

// Forward events from the iframe to the test as messages.
addEventListener("TestEventAck", event => {
  // The waiveXrays call is copied from the contentSearch.js part of
  // browser_ContentSearch.js test.  Not sure whether it's necessary here.
  sendAsyncMessage("TestMessageAck", Components.utils.waiveXrays(event.detail));
}, true, true);

// Send a message to the test when the iframe is loaded.
if (content.document.readyState == "complete") {
  sendAsyncMessage("TestIframeLoadAck");
} else {
  addEventListener("load", function onLoad(event) {
    removeEventListener("load", onLoad);
    sendAsyncMessage("TestIframeLoadAck");
  }, true, true);
}
