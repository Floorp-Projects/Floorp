/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

declTest("getActor in the regular shared loader", {
  loadInDevToolsLoader: false,

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal.domProcess;
    let parentActor = parent.getActor("TestProcessActor");
    ok(parentActor, "JSProcessActorParent should have value.");
    is(
      Cu.getRealmLocation(Cu.getGlobalForObject(parentActor)),
      "shared JSM global",
      "The JSActor module in the parent process should be loaded in the shared global"
    );

    await SpecialPowers.spawn(browser, [], async function () {
      let child = ChromeUtils.domProcessChild;
      ok(child, "DOMProcessChild should have value.");
      let childActor = child.getActor("TestProcessActor");
      ok(childActor, "JSProcessActorChild should have value.");
      is(
        Cu.getRealmLocation(Cu.getGlobalForObject(childActor)),
        "shared JSM global",
        "The JSActor module in the child process should be loaded in the shared global"
      );
    });
  },
});

declTest("getActor in the distinct DevTools loader", {
  loadInDevToolsLoader: true,

  async test(browser) {
    let parent = browser.browsingContext.currentWindowGlobal.domProcess;
    let parentActor = parent.getActor("TestProcessActor");
    ok(parentActor, "JSProcessActorParent should have value.");
    is(
      Cu.getRealmLocation(Cu.getGlobalForObject(parentActor)),
      "DevTools global",
      "The JSActor module in the parent process should be loaded in the distinct DevTools global"
    );

    await SpecialPowers.spawn(browser, [], async function () {
      let child = ChromeUtils.domProcessChild;
      ok(child, "DOMProcessChild should have value.");
      let childActor = child.getActor("TestProcessActor");
      ok(childActor, "JSProcessActorChild should have value.");
      is(
        Cu.getRealmLocation(Cu.getGlobalForObject(childActor)),
        "DevTools global",
        "The JSActor module in the child process should be loaded in the distinct DevTools global"
      );
    });
  },
});
