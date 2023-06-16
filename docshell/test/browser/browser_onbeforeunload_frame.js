"use strict";

// We need to test a lot of permutations here, and there isn't any sensible way
// to split them up or run them faster.
requestLongerTimeout(12);

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

    for (let startIdx = 0; startIdx < FRAMES.length; startIdx++) {
      info(`Testing content reload from frame ${startIdx}`);

      await doTest(actions, startIdx, (tab, frames) => {
        return SpecialPowers.spawn(frames[startIdx], [], () => {
          let eventLoopSpun = false;
          SpecialPowers.Services.tm.dispatchToMainThread(() => {
            eventLoopSpun = true;
          });

          content.location.reload();

          return { eventLoopSpun };
        });
      });
    }
  }
});

add_task(async function cleanup() {
  await TabPool.cleanup();
});
