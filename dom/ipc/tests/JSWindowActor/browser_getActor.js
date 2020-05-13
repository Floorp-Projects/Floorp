/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ACTOR_PARENT_CREATED_NOTIFICATION = "test-window-actor-parent-created";

function promiseNotification(aNotification) {
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );
  let notificationResolve;
  let notificationObserver = function observer() {
    notificationResolve();
    Services.obs.removeObserver(notificationObserver, aNotification);
  };
  return new Promise(resolve => {
    notificationResolve = resolve;
    Services.obs.addObserver(notificationObserver, aNotification);
  });
}

declTest("getActor on both sides", {
  async test(browser) {
    let parentCreationObserved = promiseNotification(
      ACTOR_PARENT_CREATED_NOTIFICATION
    );
    let parent = browser.browsingContext.currentWindowGlobal;
    ok(parent, "WindowGlobalParent should have value.");
    let actorParent = parent.getActor("TestWindow");
    is(actorParent.show(), "TestWindowParent", "actor show should have vaule.");
    is(actorParent.manager, parent, "manager should match WindowGlobalParent.");

    ok(true, "Checking that we can observe parent creation");
    await parentCreationObserved;
    ok(true, "Parent creation was observed");

    await SpecialPowers.spawn(
      browser,
      [promiseNotification.toString()],
      async function(promiseNotificationSource) {
        const ACTOR_CHILD_CREATED_NOTIFICATION =
          "test-window-actor-child-created";
        let childCreationObserved = new Function(promiseNotificationSource)(
          ACTOR_CHILD_CREATED_NOTIFICATION
        );

        let child = content.windowGlobalChild;
        ok(child, "WindowGlobalChild should have value.");
        is(
          child.isInProcess,
          false,
          "Actor should be loaded in the content process."
        );
        let actorChild = child.getActor("TestWindow");
        is(
          actorChild.show(),
          "TestWindowChild",
          "actor show should have vaule."
        );
        is(
          actorChild.manager,
          child,
          "manager should match WindowGlobalChild."
        );

        ok(true, "Checking that we can observe child creation");
        await childCreationObserved;
        ok(true, "Child creation was observed");
      }
    );
  },
});
