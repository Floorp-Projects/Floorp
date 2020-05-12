/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const ACTOR_PARENT_CREATED_NOTIFICATION = "test-process-actor-parent-created";

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
    let parent = browser.browsingContext.currentWindowGlobal.contentParent;
    ok(parent, "WindowGlobalParent should have value.");
    let actorParent = parent.getActor("TestProcessActor");
    is(
      actorParent.show(),
      "TestProcessActorParent",
      "actor show should have value."
    );
    is(
      actorParent.manager,
      parent,
      "manager should match WindowGlobalParent.contentParent"
    );

    await parentCreationObserved;
    ok(true, "Parent creation was observed");

    await SpecialPowers.spawn(
      browser,
      [promiseNotification.toString()],
      async function(promiseNotificationSource) {
        const ACTOR_CHILD_CREATED_NOTIFICATION =
          "test-process-actor-child-created";
        let childCreationObserved = new Function(promiseNotificationSource)(
          ACTOR_CHILD_CREATED_NOTIFICATION
        );
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
        await childCreationObserved;
        ok(true, "Child creation was observed");
      }
    );
  },
});
