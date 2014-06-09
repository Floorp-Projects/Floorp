const Cu = Components.utils;

const { Services } = Cu.import("resource://gre/modules/Services.jsm");

// Load a duplicated copy of the jsm to prevent messing with the currently running one
let scope = {};
Services.scriptloader.loadSubScript("resource://gre/modules/SystemAppProxy.jsm", scope);
const { SystemAppProxy } = scope;

let frame;
let customEventTarget;

let index = -1;
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
let isLoaded = false;
let n = 0;
function listener(event) {
  if (!isLoaded) {
    assert.ok(false, "Received event before the iframe is ready");
    return;
  }
  n++;
  if (n == 1) {
    assert.equal(event.type, "mozChromeEvent");
    assert.equal(event.detail.name, "first");
  } else if (n == 2) {
    assert.equal(event.type, "custom");
    assert.equal(event.detail.name, "second");

    next(); // call checkEventDispatching
  } else if (n == 3) {
    assert.equal(event.type, "custom");
    assert.equal(event.detail.name, "third");
  } else if (n == 4) {
    assert.equal(event.type, "mozChromeEvent");
    assert.equal(event.detail.name, "fourth");
  } else if (n == 5) {
    assert.equal(event.type, "custom");
    assert.equal(event.detail.name, "fifth");
    assert.equal(event.target, customEventTarget);

    next(); // call checkEventListening();
  } else {
    assert.ok(false, "Unexpected event of type " + event.type);
  }
}


let steps = [
  function waitForWebapps() {
    // We are using webapps API later in this test and we need to ensure
    // it is fully initialized before trying to use it
    let { DOMApplicationRegistry } =  Cu.import('resource://gre/modules/Webapps.jsm', {});
    DOMApplicationRegistry.registryReady.then(function () {
      next();
    });
  },

  function earlyEvents() {
    // Immediately try to send events
    SystemAppProxy.dispatchEvent({ name: "first" });
    SystemAppProxy._sendCustomEvent("custom", { name: "second" });
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
      assert(false, "Listener isn't correctly removed from the pending list");
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
      SystemAppProxy.setIsReady();
      assert.ok(true, "Frame declared as loaded");

      // Once pending events are received,
      // we will run checkEventDispatching from `listener` function
    });

    frame.setAttribute("src", "data:text/html,system app");
  },

  function checkEventDispatching() {
    // Send events after the iframe is ready,
    // they should be dispatched right away
    SystemAppProxy._sendCustomEvent("custom", { name: "third" });
    SystemAppProxy.dispatchEvent({ name: "fourth" });
    SystemAppProxy._sendCustomEvent("custom", { name: "fifth" }, false, customEventTarget);
    // Once this 5th event is received, we will run checkEventListening
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
