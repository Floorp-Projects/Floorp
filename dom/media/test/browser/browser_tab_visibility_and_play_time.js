/**
 * This test is used to ensure that invisible play time would be accumulated
 * when tab is in background. However, this test won't directly check the
 * reported telemetry result, because we can't check the snapshot histogram in
 * the content process.
 * The actual probe checking happens in `test_accumulated_play_time.html`.
 */
"use strict";

const PAGE_URL =
  "https://example.com/browser/dom/media/test/browser/file_media.html";

add_task(async function testChangingTabVisibilityAffectsInvisiblePlayTime() {
  const originalTab = gBrowser.selectedTab;
  const mediaTab = await openMediaTab(PAGE_URL);

  info(`measuring play time when tab is in foreground`);
  await startMedia({
    mediaTab,
    shouldAccumulateTime: true,
    shouldAccumulateInvisibleTime: false,
  });
  await pauseMedia(mediaTab);

  info(`measuring play time when tab is in background`);
  await BrowserTestUtils.switchTab(window.gBrowser, originalTab);
  await startMedia({
    mediaTab,
    shouldAccumulateTime: true,
    shouldAccumulateInvisibleTime: true,
  });
  await pauseMedia(mediaTab);

  BrowserTestUtils.removeTab(mediaTab);
});

/**
 * Following are helper functions.
 */
async function openMediaTab(url) {
  info(`open tab for media playback`);
  const tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser, url);
  info(`add content helper functions and variables`);
  await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    content.waitForOnTimeUpdate = element => {
      return new Promise(resolve => {
        element.addEventListener(
          "timeupdate",
          e => {
            resolve();
          },
          { once: true }
        );
      });
    };

    content.sleep = ms => {
      return new Promise(resolve => content.setTimeout(resolve, ms));
    };

    content.assertAttributeDefined = (videoChrome, checkType) => {
      ok(videoChrome[checkType] != undefined, `${checkType} exists`);
    };
    content.assertValueEqualTo = (videoChrome, checkType, expectedValue) => {
      content.assertAttributeDefined(videoChrome, checkType);
      is(
        videoChrome[checkType],
        expectedValue,
        `${checkType} equals to ${expectedValue}`
      );
    };
    content.assertValueConstantlyIncreases = async (videoChrome, checkType) => {
      content.assertAttributeDefined(videoChrome, checkType);
      const valueSnapshot = videoChrome[checkType];
      await content.waitForOnTimeUpdate(videoChrome);
      ok(
        videoChrome[checkType] > valueSnapshot,
        `${checkType} keeps increasing`
      );
    };
    content.assertValueKeptUnchanged = async (videoChrome, checkType) => {
      content.assertAttributeDefined(videoChrome, checkType);
      const valueSnapshot = videoChrome[checkType];
      await content.sleep(1000);
      ok(
        videoChrome[checkType] == valueSnapshot,
        `${checkType} keeps unchanged`
      );
    };
  });
  return tab;
}

function startMedia({
  mediaTab,
  shouldAccumulateTime,
  shouldAccumulateInvisibleTime,
}) {
  return SpecialPowers.spawn(
    mediaTab.linkedBrowser,
    [shouldAccumulateTime, shouldAccumulateInvisibleTime],
    async (accumulateTime, accumulateInvisibleTime) => {
      const video = content.document.getElementById("video");
      ok(
        await video.play().then(
          () => true,
          () => false
        ),
        "video started playing"
      );
      const videoChrome = SpecialPowers.wrap(video);
      if (accumulateTime) {
        await content.assertValueConstantlyIncreases(
          videoChrome,
          "totalPlayTime"
        );
      } else {
        await content.assertValueKeptUnchanged(videoChrome, "totalPlayTime");
      }
      if (accumulateInvisibleTime) {
        await content.assertValueConstantlyIncreases(
          videoChrome,
          "invisiblePlayTime"
        );
      } else {
        await content.assertValueKeptUnchanged(
          videoChrome,
          "invisiblePlayTime"
        );
      }
    }
  );
}

function pauseMedia(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    const video = content.document.getElementById("video");
    video.pause();
    ok(true, "video paused");
    const videoChrome = SpecialPowers.wrap(video);
    await content.assertValueKeptUnchanged(videoChrome, "totalPlayTime");
    await content.assertValueKeptUnchanged(videoChrome, "invisiblePlayTime");
  });
}
