/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

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

function teardown() {
  windowActorOptions.allFrames = false;
  delete windowActorOptions.matches;
  ChromeUtils.unregisterWindowActor("Test");
}

add_task(function test_registerWindowActor() {
  ok(ChromeUtils, "Should be able to get the ChromeUtils interface");
  ChromeUtils.registerWindowActor("Test", windowActorOptions);
  SimpleTest.doesThrow(() =>
    ChromeUtils.registerWindowActor("Test", windowActorOptions),
    "Should throw if register has duplicate name.");
  ChromeUtils.unregisterWindowActor("Test");
});

add_task(async function test_getActor() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL},
    async function(browser) {
      ChromeUtils.registerWindowActor("Test", windowActorOptions);
      let parent = browser.browsingContext.currentWindowGlobal;
      ok(parent, "WindowGlobalParent should have value.");
      let actorParent = parent.getActor("Test");
      is(actorParent.show(), "TestParent", "actor show should have vaule.");
      is(actorParent.manager, parent, "manager should match WindowGlobalParent.");

      await ContentTask.spawn(
        browser, {}, async function() {
          let child = content.window.getWindowGlobalChild();
          ok(child, "WindowGlobalChild should have value.");
          is(child.isInProcess, false, "Actor should be loaded in the content process.");
          let actorChild = child.getActor("Test");
          is(actorChild.show(), "TestChild", "actor show should have vaule.");
          is(actorChild.manager, child, "manager should match WindowGlobalChild.");
        });
      ChromeUtils.unregisterWindowActor("Test");
    });
});

add_task(async function test_asyncMessage() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL},
    async function(browser) {
      ChromeUtils.registerWindowActor("Test", windowActorOptions);
      let parent = browser.browsingContext.currentWindowGlobal;
      let actorParent = parent.getActor("Test");
      ok(actorParent, "JSWindowActorParent should have value.");

      await ContentTask.spawn(
        browser, {}, async function() {
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
        ChromeUtils.unregisterWindowActor("Test");
    });
});

add_task(async function test_asyncMessage_without_both_side_actor() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL},
    async function(browser) {
      ChromeUtils.registerWindowActor("Test", windowActorOptions);
      // If we don't create a parent actor, make sure the parent actor
      // gets created by having sent the message.
      await ContentTask.spawn(
        browser, {}, async function() {
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
        ChromeUtils.unregisterWindowActor("Test");
    });
});

add_task(async function test_events() {
  ChromeUtils.registerWindowActor("Test", windowActorOptions);
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async browser => {
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
  });
  ChromeUtils.unregisterWindowActor("Test");
});

add_task(async function test_observers() {
  ChromeUtils.registerWindowActor("Test", windowActorOptions);
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async browser => {
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
  });
  ChromeUtils.unregisterWindowActor("Test");
});

add_task(async function test_observers_with_null_data() {
  ChromeUtils.registerWindowActor("Test", windowActorOptions);
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async browser => {
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
  });
  ChromeUtils.unregisterWindowActor("Test");
});

add_task(async function test_observers_dont_notify_with_wrong_window() {
  ChromeUtils.registerWindowActor("Test", windowActorOptions);
  await BrowserTestUtils.withNewTab({gBrowser, url: URL}, async browser => {
    await ContentTask.spawn(browser, {}, async function() {
      const TOPIC = "test-js-window-actor-child-observer";
      Services.obs.notifyObservers(null, TOPIC);
      let child = content.window.getWindowGlobalChild();
      let actorChild = child.getActor("Test");
      ok(actorChild, "JSWindowActorChild should have value.");
      is(actorChild.lastObserved, undefined, "Should not receive wrong window's observer notification!");
    });
  });
  ChromeUtils.unregisterWindowActor("Test");
});

add_task(async function test_getActor_with_mismatch() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL},
    async function(browser) {
      windowActorOptions.matches = ["*://*/*"];
      ChromeUtils.registerWindowActor("Test", windowActorOptions);
      let parent = browser.browsingContext.currentWindowGlobal;
      ok(parent, "WindowGlobalParent should have value.");
      Assert.throws(() => parent.getActor("Test"),
            /NS_ERROR_NOT_AVAILABLE/, "Should throw if it doesn't match.");

      await ContentTask.spawn(
        browser, {}, async function() {
          let child = content.window.getWindowGlobalChild();
          ok(child, "WindowGlobalChild should have value.");

          Assert.throws(() => child.getActor("Test"),
            /NS_ERROR_NOT_AVAILABLE/, "Should throw if it doesn't match.");
        });
      teardown();
    });
});

add_task(async function test_getActor_with_matches() {
  await BrowserTestUtils.withNewTab({gBrowser, url: TEST_URL},
    async function(browser) {
      windowActorOptions.matches = ["*://*/*"];
      ChromeUtils.registerWindowActor("Test", windowActorOptions);
      let parent = browser.browsingContext.currentWindowGlobal;
      ok(parent.getActor("Test"), "JSWindowActorParent should have value.");

      await ContentTask.spawn(
        browser, {}, async function() {
          let child = content.window.getWindowGlobalChild();
          ok(child, "WindowGlobalChild should have value.");
          ok(child.getActor("Test"), "JSWindowActorChild should have value.");
        });

      teardown();
    });
});

add_task(async function test_getActor_with_iframe_matches() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL},
    async function(browser) {
      windowActorOptions.allFrames = true;
      windowActorOptions.matches = ["*://*/*"];
      ChromeUtils.registerWindowActor("Test", windowActorOptions);

      await ContentTask.spawn(
        browser, TEST_URL, async function(url) {
          // Create and append an iframe into the window's document.
          let frame = content.document.createElement("iframe");
          frame.src = url;
          content.document.body.appendChild(frame);
          await ContentTaskUtils.waitForEvent(frame, "load");

          is(content.window.frames.length, 1, "There should be an iframe.");
          let child = frame.contentWindow.window.getWindowGlobalChild();
          ok(child.getActor("Test"), "JSWindowActorChild should have value.");
        });
      teardown();
    });
});

add_task(async function test_getActor_with_iframe_mismatch() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL},
    async function(browser) {
      windowActorOptions.allFrames = true;
      windowActorOptions.matches = ["about:home"];
      ChromeUtils.registerWindowActor("Test", windowActorOptions);

      await ContentTask.spawn(
        browser, TEST_URL, async function(url) {
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
      teardown();
    });
});

add_task(async function test_getActor_without_allFrames() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL},
    async function(browser) {
      windowActorOptions.allFrames = false;
      ChromeUtils.registerWindowActor("Test", windowActorOptions);

      await ContentTask.spawn(
        browser, {}, async function() {
          // Create and append an iframe into the window's document.
          let frame = content.document.createElement("iframe");
          content.document.body.appendChild(frame);
          is(content.window.frames.length, 1, "There should be an iframe.");
          let child = frame.contentWindow.window.getWindowGlobalChild();
          Assert.throws(() => child.getActor("Test"),
            /NS_ERROR_NOT_AVAILABLE/, "Should throw if allFrames is false.");
        });
      ChromeUtils.unregisterWindowActor("Test");
    });
});

add_task(async function test_getActor_with_allFrames() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL},
    async function(browser) {
      windowActorOptions.allFrames = true;
      ChromeUtils.registerWindowActor("Test", windowActorOptions);

      await ContentTask.spawn(
        browser, {}, async function() {
          // Create and append an iframe into the window's document.
          let frame = content.document.createElement("iframe");
          content.document.body.appendChild(frame);
          is(content.window.frames.length, 1, "There should be an iframe.");
          let child = frame.contentWindow.window.getWindowGlobalChild();
          let actorChild = child.getActor("Test");
          ok(actorChild, "JSWindowActorChild should have value.");
        });
      ChromeUtils.unregisterWindowActor("Test");
    });
});

add_task(async function test_getActor_without_includeChrome() {
  windowActorOptions.includeChrome = false;
  let parent = window.docShell.browsingContext.currentWindowGlobal;
  ChromeUtils.registerWindowActor("Test", windowActorOptions);
  SimpleTest.doesThrow(() =>
    parent.getActor("Test"),
    "Should throw if includeChrome is false.");
  ChromeUtils.unregisterWindowActor("Test");
});

add_task(async function test_getActor_with_includeChrome() {
  windowActorOptions.includeChrome = true;
  ChromeUtils.registerWindowActor("Test", windowActorOptions);
  let parent = window.docShell.browsingContext.currentWindowGlobal;
  let actorParent = parent.getActor("Test");
  ok(actorParent, "JSWindowActorParent should have value.");
  ChromeUtils.unregisterWindowActor("Test");
});
