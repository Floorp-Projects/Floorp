const PAGE_NON_AUTOPLAY =
  "https://example.com/browser/dom/media/mediacontrol/tests/browser/file_non_autoplay.html";
const VIDEO_ID = "video";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.mediacontrol.testingevents.enabled", true],
      ["dom.suspend_inactive.enabled", true],
      ["dom.audiocontext.testing", true],
    ],
  });
});

/**
 * This test to used to test the feature that would suspend the inactive tab,
 * which currently is only used on Android.
 *
 * Normally when tab becomes inactive, we would suspend it and stop its script
 * from running. However, if a tab has a main controller, which indicates it
 * might have playng media, or waiting media keys to control media, then it
 * would not be suspended event if it's inactive.
 *
 * In addition, Note that, on Android, audio focus management is enabled by
 * default, so there is only one tab being able to play at a time, which means
 * the tab playing media always has main controller.
 */
add_task(async function testInactiveTabWouldBeSuspended() {
  info(`open a tab`);
  const tab = await createTab(PAGE_NON_AUTOPLAY);
  await assertIfWindowGetSuspended(tab, { shouldBeSuspended: false });

  info(`tab should be suspended when it becomes inactive`);
  setTabActive(tab, false);
  await assertIfWindowGetSuspended(tab, { shouldBeSuspended: true });

  info(`remove tab`);
  await tab.close();
});

add_task(async function testInactiveTabEverStartPlayingWontBeSuspended() {
  info(`open tab1 and play media`);
  const tab1 = await createTab(PAGE_NON_AUTOPLAY, { needCheck: true });
  await assertIfWindowGetSuspended(tab1, { shouldBeSuspended: false });
  await playMedia(tab1, VIDEO_ID);

  info(`tab with playing media won't be suspended when it becomes inactive`);
  setTabActive(tab1, false);
  await assertIfWindowGetSuspended(tab1, { shouldBeSuspended: false });

  info(
    `even if media is paused, keep tab running so that it could listen to media keys to control media in the future`
  );
  await pauseMedia(tab1, VIDEO_ID);
  await assertIfWindowGetSuspended(tab1, { shouldBeSuspended: false });

  info(`open tab2 and play media`);
  const tab2 = await createTab(PAGE_NON_AUTOPLAY, { needCheck: true });
  await assertIfWindowGetSuspended(tab2, { shouldBeSuspended: false });
  await playMedia(tab2, VIDEO_ID);

  info(
    `as inactive tab1 doesn't own main controller, it should be suspended again`
  );
  await assertIfWindowGetSuspended(tab1, { shouldBeSuspended: true });

  info(`remove tabs`);
  await Promise.all([tab1.close(), tab2.close()]);
});

add_task(
  async function testInactiveTabWithRunningAudioContextWontBeSuspended() {
    info(`open tab and start an audio context (AC)`);
    const tab = await createTab("about:blank");
    await startAudioContext(tab);
    await assertIfWindowGetSuspended(tab, { shouldBeSuspended: false });

    info(`tab with running AC won't be suspended when it becomes inactive`);
    setTabActive(tab, false);
    await assertIfWindowGetSuspended(tab, { shouldBeSuspended: false });

    info(`if AC has been suspended, then inactive tab should be suspended`);
    await suspendAudioContext(tab);
    await assertIfWindowGetSuspended(tab, { shouldBeSuspended: true });

    info(`remove tab`);
    await tab.close();
  }
);

/**
 * The following are helper functions.
 */
async function createTab(url, needCheck = false) {
  const tab = await createLoadedTabWrapper(url, { needCheck });
  return tab;
}

function assertIfWindowGetSuspended(tab, { shouldBeSuspended }) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [shouldBeSuspended],
    expectedSuspend => {
      const isSuspended = content.windowUtils.suspendedByBrowsingContextGroup;
      is(
        expectedSuspend,
        isSuspended,
        `window suspended state (${isSuspended}) is equal to the expected`
      );
    }
  );
}

function setTabActive(tab, isActive) {
  tab.linkedBrowser.docShellIsActive = isActive;
}

function startAudioContext(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    content.ac = new content.AudioContext();
    await new Promise(r => (content.ac.onstatechange = r));
    Assert.equal(content.ac.state, "running", `Audio context started running`);
  });
}

function suspendAudioContext(tab) {
  return SpecialPowers.spawn(tab.linkedBrowser, [], async _ => {
    await content.ac.suspend();
    Assert.equal(content.ac.state, "suspended", `Audio context is suspended`);
  });
}
