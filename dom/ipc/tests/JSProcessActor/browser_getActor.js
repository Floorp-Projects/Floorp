/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("getActor on both sides", {
  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal.contentParent;
    ok(parent, "WindowGlobalParent should have value.");
    let actorParent = parent.getActor("TestProcessActor");
    is(
      actorParent.show(),
      "TestProcessActorParent",
      "actor show should have vaule."
    );
    is(
      actorParent.manager,
      parent,
      "manager should match WindowGlobalParent.contentParent"
    );

    await SpecialPowers.spawn(browser, [], async function() {
      let child = ChromeUtils.contentChild;
      ok(child, "WindowGlobalChild should have value.");
      let actorChild = child.getActor("TestProcessActor");
      is(
        actorChild.show(),
        "TestProcessActorChild",
        "actor show should have vaule."
      );
      is(
        actorChild.manager,
        child,
        "manager should match ChromeUtils.contentChild."
      );
    });
  },
});
