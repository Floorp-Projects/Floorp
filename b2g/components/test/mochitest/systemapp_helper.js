var Cu = Components.utils;

const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

// Load a duplicated copy of the jsm to prevent messing with the currently running one
var scope = {};
Services.scriptloader.loadSubScript("resource://gre/modules/SystemAppProxy.jsm", scope);
const { SystemAppProxy } = scope;

var frame;
var customEventTarget;

var index = -1;
function next() {
  index++;
  if (index >= steps.length) {
    assert.ok(false, "Shouldn't get here!");
    return;
  }
  try {
    steps[index]();
  } catch(ex) {
    assert.ok(false, "Caught exception: " + ex);
  }
}

// Listen for events received by the system app document
// to ensure that we receive all of them, in an expected order and time
var isLoaded = false;
var isReady = false;
var n = 0;
function listener(event) {
  if (!isLoaded) {
    assert.ok(false, "Received event before the iframe is loaded");
    return;
  }
  n++;
  if (n == 1) {
    assert.equal(event.type, "mozChromeEvent");
    assert.equal(event.detail.name, "first");
  } else if (n == 2) {
    assert.equal(event.type, "custom");
    assert.equal(event.detail.name, "second");

    next(); // call checkEventPendingBeforeLoad
  } else if (n == 3) {
    if (!isReady) {
      assert.ok(false, "Received event before the iframe is loaded");
      return;
    }

    assert.equal(event.type, "custom");
    assert.equal(event.detail.name, "third");
  } else if (n == 4) {
    if (!isReady) {
      assert.ok(false, "Received event before the iframe is loaded");
      return;
    }

    assert.equal(event.type, "mozChromeEvent");
    assert.equal(event.detail.name, "fourth");

    next(); // call checkEventDispatching
  } else if (n == 5) {
    assert.equal(event.type, "custom");
    assert.equal(event.detail.name, "fifth");
  } else if (n === 6) {
    assert.equal(event.type, "mozChromeEvent");
    assert.equal(event.detail.name, "sixth");
  } else if (n === 7) {
    assert.equal(event.type, "custom");
    assert.equal(event.detail.name, "seventh");
    assert.equal(event.target, customEventTarget);

    next(); // call checkEventListening();
  } else {
    assert.ok(false, "Unexpected event of type " + event.type);
  }
}


var steps = [
  function earlyEvents() {
    // Immediately try to send events
    SystemAppProxy._sendCustomEvent("mozChromeEvent", { name: "first" }, true);
    SystemAppProxy._sendCustomEvent("custom", { name: "second" }, true);
    next();
  },

  function createFrame() {
    // Create a fake system app frame
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    let doc = win.document;
    frame = doc.createElement("iframe");
    doc.documentElement.appendChild(frame);

    customEventTarget = frame.contentDocument.body;

    // Ensure that events are correctly sent to the frame.
    // `listener` is going to call next()
    frame.contentWindow.addEventListener("mozChromeEvent", listener);
    frame.contentWindow.addEventListener("custom", listener);

    // Ensure that listener being registered before the system app is ready
    // are correctly removed from the pending list
    function removedListener() {
      assert.ok(false, "Listener isn't correctly removed from the pending list");
    }
    SystemAppProxy.addEventListener("mozChromeEvent", removedListener);
    SystemAppProxy.removeEventListener("mozChromeEvent", removedListener);

    // Register it to the JSM
    SystemAppProxy.registerFrame(frame);
    assert.ok(true, "Frame created and registered");

    frame.contentWindow.addEventListener("load", function onload() {
      frame.contentWindow.removeEventListener("load", onload);
      assert.ok(true, "Frame document loaded");

      // Declare that the iframe is now loaded.
      // That should dispatch early events
      isLoaded = true;
      SystemAppProxy.setIsLoaded();
      assert.ok(true, "Frame declared as loaded");

      let gotFrame = SystemAppProxy.getFrame();
      assert.equal(gotFrame, frame, "getFrame returns the frame we passed");

      // Once pending events are received,
      // we will run checkEventDispatching from `listener` function
    });

    frame.setAttribute("src", "data:text/html,system app");
  },

  function checkEventPendingBeforeLoad() {
    // Frame is loaded but not ready,
    // these events should queue before the System app is ready.
    SystemAppProxy._sendCustomEvent("custom", { name: "third" });
    SystemAppProxy.dispatchEvent({ name: "fourth" });

    isReady = true;
    SystemAppProxy.setIsReady();
    // Once this 4th event is received, we will run checkEventDispatching
  },

  function checkEventDispatching() {
    // Send events after the iframe is ready,
    // they should be dispatched right away
    SystemAppProxy._sendCustomEvent("custom", { name: "fifth" });
    SystemAppProxy.dispatchEvent({ name: "sixth" });
    SystemAppProxy._sendCustomEvent("custom", { name: "seventh" }, false, customEventTarget);
    // Once this 7th event is received, we will run checkEventListening
  },

  function checkEventListening() {
    SystemAppProxy.addEventListener("mozContentEvent", function onContentEvent(event) {
      assert.equal(event.detail.name, "first-content", "received a system app event");
      SystemAppProxy.removeEventListener("mozContentEvent", onContentEvent);

      next();
    });
    let win = frame.contentWindow;
    win.dispatchEvent(new win.CustomEvent("mozContentEvent", { detail: {name: "first-content"} }));
  },

  function endOfTest() {
    frame.remove();
    sendAsyncMessage("finish");
  }
];

next();
