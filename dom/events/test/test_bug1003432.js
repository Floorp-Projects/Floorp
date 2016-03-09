addEventListener("foobar",
  function(evt) {
    postMessage(
      {
        type: evt.type,
        bubbles: evt.bubbles,
        cancelable: evt.cancelable,
        detail: evt.detail
      });
  }, true);

addEventListener("message",
  function(evt) {
    // Test the constructor of CustomEvent
    var e = new CustomEvent("foobar",
      {bubbles:true, cancelable: true, detail:"test"});
    dispatchEvent(e);

    // Test initCustomEvent
    e = new CustomEvent("foobar");
    e.initCustomEvent("foobar", true, true, "test");
    dispatchEvent(e);
  }, true);
