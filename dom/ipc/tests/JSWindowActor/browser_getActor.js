/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("getActor on both sides", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal;
    ok(parent, "WindowGlobalParent should have value.");
    let actorParent = parent.getActor("TestWindow");
    is(actorParent.show(), "TestWindowParent", "actor show should have vaule.");
    is(actorParent.manager, parent, "manager should match WindowGlobalParent.");

    await SpecialPowers.spawn(browser, [], async function() {
      let child = content.windowGlobalChild;
      ok(child, "WindowGlobalChild should have value.");
      is(
        child.isInProcess,
        false,
        "Actor should be loaded in the content process."
      );
      let actorChild = child.getActor("TestWindow");
      is(actorChild.show(), "TestWindowChild", "actor show should have vaule.");
      is(actorChild.manager, child, "manager should match WindowGlobalChild.");
    });
  },
});
