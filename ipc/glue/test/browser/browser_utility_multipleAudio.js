/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function runTest(expectUtility) {
  info(
    `Running tests with decoding from Utility or RDD: expectUtility=${expectUtility}`
  );

  // Utility should now be the default, so dont toggle the pref unless we test
  // RDD
  if (!expectUtility) {
    await SpecialPowers.pushPrefEnv({
      set: [["media.utility-process.enabled", expectUtility]],
    });
  }

  const platform = Services.appinfo.OS;

  for (let { src, expectations } of audioTestData()) {
    if (!(platform in expectations)) {
      info(`Skipping ${src} for ${platform}`);
      continue;
    }

    info(`Add media tabs: ${src}`);
    let tabs = [await addMediaTab(src), await addMediaTab(src)];
    let playback = [];

    info("Play tabs");
    for (let tab of tabs) {
      playback.push(play(tab, expectUtility ? expectations[platform] : "RDD"));
    }

    info("Wait all playback");
    await Promise.all(playback);

    let allstop = [];
    info("Stop tabs");
    for (let tab of tabs) {
      allstop.push(stop(tab));
    }

    info("Wait all stop");
    await Promise.all(allstop);

    let remove = [];
    info("Remove tabs");
    for (let tab of tabs) {
      remove.push(BrowserTestUtils.removeTab(tab));
    }

    info("Wait all tabs to be removed");
    await Promise.all(remove);
  }
}

add_task(async function testAudioDecodingInUtility() {
  await runTest(true);
});

add_task(async function testAudioDecodingInRDD() {
  await runTest(false);
});
