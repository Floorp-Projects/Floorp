/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const URL = "about:blank";
let windowActorOptions = {
  parent: {
    moduleURI: "resource://testing-common/TestParent.jsm",
  },
  child: {
    moduleURI: "resource://testing-common/TestChild.jsm",
  },
};

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
            actorChild.sendAsyncMessage("Test", "init", {});
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
            actorChild.sendAsyncMessage("Test", "init", {});
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
