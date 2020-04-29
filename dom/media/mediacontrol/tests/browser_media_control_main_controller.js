/* eslint-disable no-undef */
"use strict";

// Import this in order to use `triggerPictureInPicture()`.
/* import-globals-from ../../../../toolkit/components/pictureinpicture/tests/head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/toolkit/components/pictureinpicture/tests/head.js",
  this
);

const PAGE_NON_AUTOPLAY =
  "https://example.com/browser/dom/media/mediacontrol/tests/file_non_autoplay.html";

const testVideoId = "video";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.mediacontrol.testingevents.enabled", true],
      ["dom.media.mediasession.enabled", true],
    ],
  });
});

/**
 * This test is used to check in different situaition if we can determine the
 * main controller correctly that is the controller which can receive media
 * control keys and show its metadata on the virtual control interface.
 *
 * We will assign different metadata for each tab and know which tab is the main
 * controller by checking main controller's metadata.
 *
 * We will always choose the last tab which plays media as the main controller,
 * and maintain a list by the order of playing media. If the top element in the
 * list has been removed, then we will use the last element in the list as the
 * main controller.
 *
 * Eg. tab1 plays first, then tab2 plays, then tab3 plays, the list would be
 * like [tab1, tab2, tab3] and the main controller would be tab3. If tab3 has
 * been closed, then the list would become [tab1, tab2] and the tab2 would be
 * the main controller.
 */
add_task(async function testDeterminingMainController() {
  info(`open three different tabs`);
  const tab0 = await createTabAndLoad(PAGE_NON_AUTOPLAY);
  const tab1 = await createTabAndLoad(PAGE_NON_AUTOPLAY);
  const tab2 = await createTabAndLoad(PAGE_NON_AUTOPLAY);

  /**
   * part1 : [] -> [tab0] -> [tab0, tab1] -> [tab0, tab1, tab2]
   */
  info(`# [] -> [tab0] -> [tab0, tab1] -> [tab0, tab1, tab2] #`);
  info(`set different metadata for each tab`);
  await setMediaMetadataForTabs([tab0, tab1, tab2]);

  info(`start media for tab0, main controller should become tab0`);
  await makeTabBecomeMainController(tab0);

  info(`currrent metadata should be equal to tab0's metadata`);
  await isCurrentMetadataEqualTo(tab0.metadata);

  info(`start media for tab1, main controller should become tab1`);
  await makeTabBecomeMainController(tab1);

  info(`currrent metadata should be equal to tab1's metadata`);
  await isCurrentMetadataEqualTo(tab1.metadata);

  info(`start media for tab2, main controller should become tab2`);
  await makeTabBecomeMainController(tab2);

  info(`currrent metadata should be equal to tab2's metadata`);
  await isCurrentMetadataEqualTo(tab2.metadata);

  /**
   * part2 : [tab0, tab1, tab2] -> [tab0, tab2, tab1] -> [tab2, tab1, tab0]
   */
  info(`# [tab0, tab1, tab2] -> [tab0, tab2, tab1] -> [tab2, tab1, tab0] #`);
  info(`start media for tab1, main controller should become tab1`);
  await makeTabBecomeMainController(tab1);

  info(`currrent metadata should be equal to tab1's metadata`);
  await isCurrentMetadataEqualTo(tab1.metadata);

  info(`start media for tab0, main controller should become tab0`);
  await makeTabBecomeMainController(tab0);

  info(`currrent metadata should be equal to tab0's metadata`);
  await isCurrentMetadataEqualTo(tab0.metadata);

  /**
   * part3 : [tab2, tab1, tab0] -> [tab2, tab1] -> [tab2] -> []
   */
  info(`# [tab2, tab1, tab0] -> [tab2, tab1] -> [tab2] -> [] #`);
  info(`remove tab0 and wait until main controller changes`);
  await Promise.all([
    waitUntilMainMediaControllerChanged(),
    BrowserTestUtils.removeTab(tab0),
  ]);

  info(`currrent metadata should be equal to tab1's metadata`);
  await isCurrentMetadataEqualTo(tab1.metadata);

  info(`remove tab1 and wait until main controller changes`);
  await Promise.all([
    waitUntilMainMediaControllerChanged(),
    BrowserTestUtils.removeTab(tab1),
  ]);

  info(`currrent metadata should be equal to tab2's metadata`);
  await isCurrentMetadataEqualTo(tab2.metadata);

  info(`remove tab2 and wait until main controller changes`);
  await Promise.all([
    waitUntilMainMediaControllerChanged(),
    BrowserTestUtils.removeTab(tab2),
  ]);
  isCurrentMetadataEmpty();
});

add_task(async function testPIPControllerIsAlwaysMainController() {
  info(`open two different tabs`);
  const tab0 = await createTabAndLoad(PAGE_NON_AUTOPLAY);
  const tab1 = await createTabAndLoad(PAGE_NON_AUTOPLAY);

  info(`set different metadata for each tab`);
  await setMediaMetadataForTabs([tab0, tab1]);

  info(`start media for tab0, main controller should become tab0`);
  await makeTabBecomeMainController(tab0);

  info(`currrent metadata should be equal to tab0's metadata`);
  await isCurrentMetadataEqualTo(tab0.metadata);

  info(`trigger Picture-in-Picture mode for tab0`);
  const winPIP = await triggerPictureInPicture(tab0.linkedBrowser, testVideoId);

  info(`start media for tab1, main controller should still be tab0`);
  await playMediaAndWaitUntilRegisteringController(tab1, testVideoId);

  info(`currrent metadata should be equal to tab0's metadata`);
  await isCurrentMetadataEqualTo(tab0.metadata);

  info(`remove tab0 and wait until main controller changes`);
  await BrowserTestUtils.closeWindow(winPIP);
  await Promise.all([
    waitUntilMainMediaControllerChanged(),
    BrowserTestUtils.removeTab(tab0),
  ]);

  info(`currrent metadata should be equal to tab1's metadata`);
  await isCurrentMetadataEqualTo(tab1.metadata);

  info(`remove tab1 and wait until main controller changes`);
  await Promise.all([
    waitUntilMainMediaControllerChanged(),
    BrowserTestUtils.removeTab(tab1),
  ]);
  isCurrentMetadataEmpty();
});

/**
 * The following are helper functions
 */
async function setMediaMetadataForTabs(tabs) {
  for (let idx = 0; idx < tabs.length; idx++) {
    const tabName = "tab" + idx;
    info(`create metadata for ${tabName}`);
    tabs[idx].metadata = {
      title: tabName,
      artist: tabName,
      album: tabName,
      artwork: [{ src: tabName, sizes: "128x128", type: "image/jpeg" }],
    };
    const spawn = SpecialPowers.spawn(
      tabs[idx].linkedBrowser,
      [tabs[idx].metadata],
      data => {
        content.navigator.mediaSession.metadata = new content.MediaMetadata(
          data
        );
      }
    );
    await Promise.all([spawn, waitUntilControllerMetadataChanged()]);
  }
}

function makeTabBecomeMainController(tab) {
  const playPromise = SpecialPowers.spawn(
    tab.linkedBrowser,
    [testVideoId],
    async Id => {
      const video = content.document.getElementById(Id);
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      // If media has been started, we would stop media first and then start it
      // again, which would make controller's playback state change to `playing`
      // again and result in updating new main controller.
      if (!video.paused) {
        video.pause();
        info(`wait until media stops`);
        await new Promise(r => (video.onpause = r));
      }
      info(`start media`);
      return video.play();
    }
  );
  return Promise.all([playPromise, waitUntilMainMediaControllerChanged()]);
}

function playMediaAndWaitUntilRegisteringController(tab, elementId) {
  const playPromise = SpecialPowers.spawn(
    tab.linkedBrowser,
    [elementId],
    Id => {
      const video = content.document.getElementById(Id);
      if (!video) {
        ok(false, `can't get the media element!`);
      }
      return video.play();
    }
  );
  return Promise.all([waitUntilMediaControllerAmountChanged(), playPromise]);
}
