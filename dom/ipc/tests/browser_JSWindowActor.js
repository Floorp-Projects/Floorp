/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const URL = "about:blank";

add_task(function test_registerWindowActor() {
  let windowActorOptions = {
    parent: {
      moduleURI: "resource://testing-common/TestParent.jsm",
    },
    child: {
      moduleURI: "resource://testing-common/TestChild.jsm",
    },
  };

  ok(ChromeUtils, "Should be able to get the ChromeUtils interface");
  ChromeUtils.registerWindowActor("Test", windowActorOptions);
  SimpleTest.doesThrow(() =>
    ChromeUtils.registerWindowActor("Test", windowActorOptions),
    "Should throw if register has duplicate name.");
});

add_task(async function() {
  await BrowserTestUtils.withNewTab({gBrowser, url: URL},
    async function(browser) {
      let parent = browser.browsingContext.currentWindowGlobal;
      isnot(parent, null, "WindowGlobalParent should have value.");
      let actorParent = parent.getActor("Test");
      is(actorParent.show(), "TestParent", "actor show should have vaule.");
      is(actorParent.manager, parent, "manager should match WindowGlobalParent.");

      await ContentTask.spawn(
        browser, {}, async function() {
          let child = content.window.getWindowGlobalChild();
          isnot(child, null, "WindowGlobalChild should have value.");
          is(child.isInProcess, false, "Actor should be loaded in the content process.");
          let actorChild = child.getActor("Test");
          is(actorChild.show(), "TestChild", "actor show should have vaule.");
          is(actorChild.manager, child, "manager should match WindowGlobalChild.");
        });
    });
});
