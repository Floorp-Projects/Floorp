"use strict";

// We need to test a lot of permutations here, and there isn't any sensible way
// to split them up or run them faster.
requestLongerTimeout(6);

Services.scriptloader.loadSubScript(
  getRootDirectory(gTestPath) + "head_browser_onbeforeunload.js",
  this
);

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  for (let actions of PERMUTATIONS) {
    info(
      `Testing frame actions: [${actions.map(action =>
        ACTION_NAMES.get(action)
      )}]`
    );

    info(`Testing tab close from parent process`);
    await doTest(actions, -1, (tab, frames) => {
      let eventLoopSpun = false;
      Services.tm.dispatchToMainThread(() => {
        eventLoopSpun = true;
      });

      BrowserTestUtils.removeTab(tab);

      let result = { eventLoopSpun };

      // Make an extra couple of trips through the event loop to give us time
      // to process SpecialPowers.spawn responses before resolving.
      return new Promise(resolve => {
        executeSoon(() => {
          executeSoon(() => resolve(result));
        });
      });
    });
  }
});

add_task(async function cleanup() {
  await TabPool.cleanup();
});
