/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// This test opens and closes a large number of windows, which can be slow
// especially on debug builds. This decreases the likelihood of the test timing
// out.
requestLongerTimeout(4);

const URL = "about:blank";
const TEST_URL = "http://test2.example.org/";
let windowActorOptions = {
  parent: {
    moduleURI: "resource://testing-common/TestParent.jsm",
  },
  child: {
    moduleURI: "resource://testing-common/TestChild.jsm",

    events: {
      "mozshowdropdown": {},
    },

    observers: [
      "test-js-window-actor-child-observer",
    ],
  },
};

function declTest(name, cfg) {
  let {
    url = "about:blank",
    allFrames = false,
    includeChrome = false,
    matches,
    remoteTypes,
    fission,
    test,
  } = cfg;

  // Build the actor options object which will be used to register & unregister our window actor.
  let actorOptions = {
    parent: Object.assign({}, windowActorOptions.parent),
    child: Object.assign({}, windowActorOptions.child),
  };
  actorOptions.allFrames = allFrames;
  actorOptions.includeChrome = includeChrome;
  if (matches !== undefined) {
    actorOptions.matches = matches;
  }
  if (remoteTypes !== undefined) {
    actorOptions.remoteTypes = remoteTypes;
  }

  // Add a new task for the actor test declared here.
  add_task(async function() {
    info("Entering test: " + name);

    // Create a fresh window with the correct settings, and register our actor.
    let win = await BrowserTestUtils.openNewBrowserWindow({remote: true, fission});
    ChromeUtils.registerWindowActor("Test", actorOptions);

    // Wait for the provided URL to load in our browser
    let browser = win.gBrowser.selectedBrowser;
    BrowserTestUtils.loadURI(browser, url);
    await BrowserTestUtils.browserLoaded(browser);

    // Run the provided test
    info("browser ready");
    await Promise.resolve(test(browser, win));

    // Clean up after we're done.
    ChromeUtils.unregisterWindowActor("Test");
    await BrowserTestUtils.closeWindow(win);

    info("Exiting test: " + name);
  });
}

declTest("double register", {
  async test() {
    SimpleTest.doesThrow(() =>
      ChromeUtils.registerWindowActor("Test", windowActorOptions),
      "Should throw if register has duplicate name.");
  },
});

declTest("getActor on both sides", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    ok(parent, "WindowGlobalParent should have value.");
    let actorParent = parent.getActor("Test");
    is(actorParent.show(), "TestParent", "actor show should have vaule.");
    is(actorParent.manager, parent, "manager should match WindowGlobalParent.");

    await ContentTask.spawn(browser, {}, async function() {
      let child = content.window.getWindowGlobalChild();
      ok(child, "WindowGlobalChild should have value.");
      is(child.isInProcess, false, "Actor should be loaded in the content process.");
      let actorChild = child.getActor("Test");
      is(actorChild.show(), "TestChild", "actor show should have vaule.");
      is(actorChild.manager, child, "manager should match WindowGlobalChild.");
    });
  },
});

declTest("asyncMessage testing", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("Test");
    ok(actorParent, "JSWindowActorParent should have value.");

    await ContentTask.spawn(browser, {}, async function() {
      let child = content.window.getWindowGlobalChild();
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");

      let promise = new Promise(resolve => {
        actorChild.sendAsyncMessage("init", {});
        actorChild.done = (data) => resolve(data);
      }).then(data => {
        ok(data.initial, "Initial should be true.");
        ok(data.toParent, "ToParent should be true.");
        ok(data.toChild, "ToChild should be true.");
      });

      await promise;
    });
  },
});

declTest("sendQuery testing", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("Test");
    ok(actorParent, "JSWindowActorParent should have value.");

    let {result} = await actorParent.sendQuery("asyncAdd", {a: 10, b: 20});
    is(result, 30);
  },
});

declTest("asyncMessage without both sides", {
  async test(browser) {
    // If we don't create a parent actor, make sure the parent actor
    // gets created by having sent the message.
    await ContentTask.spawn(browser, {}, async function() {
      let child = content.window.getWindowGlobalChild();
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");

      let promise = new Promise(resolve => {
        actorChild.sendAsyncMessage("init", {});
        actorChild.done = (data) => resolve(data);
      }).then(data => {
        ok(data.initial, "Initial should be true.");
        ok(data.toParent, "ToParent should be true.");
        ok(data.toChild, "ToChild should be true.");
      });

      await promise;
    });
  },
});

declTest("test event triggering actor creation", {
  async test(browser) {
    // Add a select element to the DOM of the loaded document.
    await ContentTask.spawn(browser, {}, async function() {
      content.document.body.innerHTML += `
        <select id="testSelect">
          <option>A</option>
          <option>B</option>
        </select>`;
    });

    // Wait for the observer notification.
    let observePromise = new Promise(resolve => {
      const TOPIC = "test-js-window-actor-parent-event";
      Services.obs.addObserver(function obs(subject, topic, data) {
        is(topic, TOPIC, "topic matches");

        Services.obs.removeObserver(obs, TOPIC);
        resolve({subject, data});
      }, TOPIC);
    });

    // Click on the select to show the dropdown.
    await BrowserTestUtils.synthesizeMouseAtCenter("#testSelect", {}, browser);

    // Wait for the observer notification to fire, and inspect the results.
    let {subject, data} = await observePromise;
    is(data, "mozshowdropdown");

    let parent = browser.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("Test");
    ok(actorParent, "JSWindowActorParent should have value.");
    is(subject.wrappedJSObject, actorParent, "Should have been recieved on the right actor");
  },
});

declTest("test observer triggering actor creation", {
  async test(browser) {
    await ContentTask.spawn(browser, {}, async function() {
      const TOPIC = "test-js-window-actor-child-observer";
      Services.obs.notifyObservers(content.window, TOPIC, "dataString");

      let child = content.window.getWindowGlobalChild();
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");
      let {subject, topic, data} = actorChild.lastObserved;

      is(subject.getWindowGlobalChild().getActor("Test"), actorChild, "Should have been recieved on the right actor");
      is(topic, TOPIC, "Topic matches");
      is(data, "dataString", "Data matches");
    });
  },
});

declTest("test observers with null data", {
  async test(browser) {
    await ContentTask.spawn(browser, {}, async function() {
      const TOPIC = "test-js-window-actor-child-observer";
      Services.obs.notifyObservers(content.window, TOPIC);

      let child = content.window.getWindowGlobalChild();
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");
      let {subject, topic, data} = actorChild.lastObserved;

      is(subject.getWindowGlobalChild().getActor("Test"), actorChild, "Should have been recieved on the right actor");
      is(topic, TOPIC, "Topic matches");
      is(data, null, "Data matches");
    });
  },
});

declTest("observers don't notify with wrong window", {
  async test(browser) {
    await ContentTask.spawn(browser, {}, async function() {
      const TOPIC = "test-js-window-actor-child-observer";
      Services.obs.notifyObservers(null, TOPIC);
      let child = content.window.getWindowGlobalChild();
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");
      is(actorChild.lastObserved, undefined, "Should not receive wrong window's observer notification!");
    });
  },
});

declTest("getActor with mismatch", {
  matches: ["*://*/*"],

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    ok(parent, "WindowGlobalParent should have value.");
    Assert.throws(() => parent.getActor("Test"),
          /NS_ERROR_NOT_AVAILABLE/, "Should throw if it doesn't match.");

    await ContentTask.spawn(browser, {}, async function() {
      let child = content.window.getWindowGlobalChild();
      ok(child, "WindowGlobalChild should have value.");

      Assert.throws(() => child.getActor("Test"),
        /NS_ERROR_NOT_AVAILABLE/, "Should throw if it doesn't match.");
    });
  },
});

declTest("getActor with matches", {
  matches: ["*://*/*"],
  url: TEST_URL,

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    ok(parent.getActor("Test"), "JSWindowActorParent should have value.");

    await ContentTask.spawn(browser, {}, async function() {
      let child = content.window.getWindowGlobalChild();
      ok(child, "WindowGlobalChild should have value.");
      ok(child.getActor("Test"), "JSWindowActorChild should have value.");
    });
  },
});

declTest("getActor with iframe matches", {
  allFrames: true,
  matches: ["*://*/*"],

  async test(browser) {
    await ContentTask.spawn(browser, TEST_URL, async function(url) {
      // Create and append an iframe into the window's document.
      let frame = content.document.createElement("iframe");
      frame.src = url;
      content.document.body.appendChild(frame);
      await ContentTaskUtils.waitForEvent(frame, "load");

      is(content.window.frames.length, 1, "There should be an iframe.");
      let child = frame.contentWindow.window.getWindowGlobalChild();
      ok(child.getActor("Test"), "JSWindowActorChild should have value.");
    });
  },
});

declTest("getActor with iframe mismatch", {
  allFrames: true,
  matches: ["about:home"],

  async test(browser) {
    await ContentTask.spawn(browser, TEST_URL, async function(url) {
      // Create and append an iframe into the window's document.
      let frame = content.document.createElement("iframe");
      frame.src = url;
      content.document.body.appendChild(frame);
      await ContentTaskUtils.waitForEvent(frame, "load");

      is(content.window.frames.length, 1, "There should be an iframe.");
      let child = frame.contentWindow.window.getWindowGlobalChild();
      Assert.throws(() => child.getActor("Test"),
        /NS_ERROR_NOT_AVAILABLE/, "Should throw if it doesn't match.");
    });
  },
});

declTest("getActor with remoteType match", {
  remoteTypes: ["web"],

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    ok(parent.getActor("Test"), "JSWindowActorParent should have value.");

    await ContentTask.spawn(browser, {}, async function() {
      let child = content.window.getWindowGlobalChild();
      ok(child, "WindowGlobalChild should have value.");
      ok(child.getActor("Test"), "JSWindowActorChild should have value.");
    });
  },
});

declTest("getActor with remoteType mismatch", {
  remoteTypes: ["privileged"],
  url: TEST_URL,

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    Assert.throws(() => parent.getActor("Test"),
          /NS_ERROR_NOT_AVAILABLE/, "Should throw if its remoteTypes don't match.");

    await ContentTask.spawn(browser, {}, async function() {
      let child = content.window.getWindowGlobalChild();
      ok(child, "WindowGlobalChild should have value.");
      Assert.throws(() => child.getActor("Test"),
          /NS_ERROR_NOT_AVAILABLE/, "Should throw if its remoteTypes don't match.");
    });
  },
});

declTest("getActor without allFrames", {
  allFrames: false,

  async test(browser) {
    await ContentTask.spawn(browser, {}, async function() {
      // Create and append an iframe into the window's document.
      let frame = content.document.createElement("iframe");
      content.document.body.appendChild(frame);
      is(content.window.frames.length, 1, "There should be an iframe.");
      let child = frame.contentWindow.window.getWindowGlobalChild();
      Assert.throws(() => child.getActor("Test"),
          /NS_ERROR_NOT_AVAILABLE/, "Should throw if allFrames is false.");
    });
  },
});

declTest("getActor with allFrames", {
  allFrames: true,

  async test(browser) {
    await ContentTask.spawn(browser, {}, async function() {
      // Create and append an iframe into the window's document.
      let frame = content.document.createElement("iframe");
      content.document.body.appendChild(frame);
      is(content.window.frames.length, 1, "There should be an iframe.");
      let child = frame.contentWindow.window.getWindowGlobalChild();
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");
    });
  },
});

declTest("getActor without includeChrome", {
  includeChrome: false,

  async test(_browser, win) {
    let parent = win.docShell.browsingContext.currentWindowGlobal;
    SimpleTest.doesThrow(() =>
      parent.getActor("Test"),
      "Should throw if includeChrome is false.");
  },
});

declTest("getActor with includeChrome", {
  includeChrome: true,

  async test(_browser, win) {
    let parent = win.docShell.browsingContext.currentWindowGlobal;
    let actorParent = parent.getActor("Test");
    ok(actorParent, "JSWindowActorParent should have value.");
  },
});
