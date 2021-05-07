/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PRIORITY_SET_TOPIC =
  "process-priority-manager:TEST-ONLY:process-priority-set";

// Copied from Hal.cpp
const PROCESS_PRIORITY_FOREGROUND = "FOREGROUND";
const PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE = "BACKGROUND_PERCEIVABLE";
const PROCESS_PRIORITY_BACKGROUND = "BACKGROUND";

// This is how many milliseconds we'll wait for a process priority
// change before we assume that it's just not happening.
const WAIT_FOR_CHANGE_TIME_MS = 2000;

// A convenience function for getting the child ID from a browsing context.
function browsingContextChildID(bc) {
  return bc.currentWindowGlobal?.domProcess.childID;
}

/**
 * This class is responsible for watching process priority changes, and
 * mapping them to tabs in a single window.
 */
class TabPriorityWatcher {
  /**
   * Constructing a TabPriorityWatcher should happen before any tests
   * start when there's only a single tab in the window.
   *
   * Callers must call `destroy()` on any instance that is constructed
   * when the test is completed.
   *
   * @param tabbrowser (<tabbrowser>)
   *   The tabbrowser (gBrowser) for the window to be tested.
   */
  constructor(tabbrowser) {
    this.tabbrowser = tabbrowser;
    Assert.equal(
      tabbrowser.tabs.length,
      1,
      "TabPriorityWatcher must be constructed in a window " +
        "with a single tab to start."
    );

    // This maps from childIDs to process priorities.
    this.priorityMap = new Map();

    // The keys in this map are childIDs we're not expecting to change.
    // Each value is either null (if no change has been seen) or the
    // priority that the process changed to.
    this.noChangeChildIDs = new Map();

    Services.obs.addObserver(this, PRIORITY_SET_TOPIC);
  }

  /**
   * Cleans up lingering references for an instance of
   * TabPriorityWatcher to avoid leaks. This should be called when
   * finishing the test.
   */
  destroy() {
    Services.obs.removeObserver(this, PRIORITY_SET_TOPIC);
  }

  /**
   * This returns a Promise that resolves when the process with
   * the given childID reaches the given priority.
   * This will eventually time out if that priority is never reached.
   *
   * @param childID
   *   The childID of the process to wait on.
   * @param expectedPriority (String)
   *   One of the PROCESS_PRIORITY_ constants defined at the
   *   top of this file.
   * @return Promise
   * @resolves undefined
   *   Once the browser reaches the expected priority.
   */
  async waitForPriorityChange(childID, expectedPriority) {
    await TestUtils.waitForCondition(() => {
      let currentPriority = this.priorityMap.get(childID);
      if (currentPriority == expectedPriority) {
        Assert.ok(
          true,
          `Process with child ID ${childID} reached expected ` +
            `priority: ${currentPriority}`
        );
        return true;
      }
      return false;
    }, `Waiting for process with child ID ${childID} to reach priority ${expectedPriority}`);
  }

  /**
   * Returns a Promise that resolves after a duration of
   * WAIT_FOR_CHANGE_TIME_MS. During that time, if the process
   * with the passed in child ID changes priority, a test
   * failure will be registered.
   *
   * @param childID
   *   The childID of the process that we expect to not change priority.
   * @return Promise
   * @resolves undefined
   *   Once the WAIT_FOR_CHANGE_TIME_MS duration has passed.
   */
  async ensureNoPriorityChange(childID) {
    this.noChangeChildIDs.set(childID, null);
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, WAIT_FOR_CHANGE_TIME_MS));
    let priority = this.noChangeChildIDs.get(childID);
    Assert.equal(
      priority,
      null,
      `Should have seen no process priority change for child ID ${childID}`
    );
    this.noChangeChildIDs.delete(childID);
  }

  /**
   * This returns a Promise that resolves when all of the processes
   * of the browsing contexts in the browsing context tree
   * of a particular <browser> have reached a particular priority.
   * This will eventually time out if that priority is never reached.
   *
   * @param browser (<browser>)
   *   The <browser> to get the BC tree from.
   * @param expectedPriority (String)
   *   One of the PROCESS_PRIORITY_ constants defined at the
   *   top of this file.
   * @return Promise
   * @resolves undefined
   *   Once the browser reaches the expected priority.
   */
  async waitForBrowserTreePriority(browser, expectedPriority) {
    let childIDs = new Set(
      browser.browsingContext
        .getAllBrowsingContextsInSubtree()
        .map(browsingContextChildID)
    );
    let promises = [];
    for (let childID of childIDs) {
      let currentPriority = this.priorityMap.get(childID);

      promises.push(
        currentPriority == expectedPriority
          ? this.ensureNoPriorityChange(childID)
          : this.waitForPriorityChange(childID, expectedPriority)
      );
    }

    await Promise.all(promises);
  }

  /**
   * Synchronously returns the priority of a particular child ID.
   *
   * @param childID
   *   The childID to get the content process priority for.
   * @return String
   *   The priority of the child ID's process.
   */
  currentPriority(childID) {
    return this.priorityMap.get(childID);
  }

  /**
   * A utility function that takes a string passed via the
   * PRIORITY_SET_TOPIC observer notification and extracts the
   * childID and priority string.
   *
   * @param ppmDataString (String)
   *   The string data passed through the PRIORITY_SET_TOPIC observer
   *   notification.
   * @return Object
   *   An object with the following properties:
   *
   *   childID (Number)
   *     The ID of the content process that changed priority.
   *
   *   priority (String)
   *     The priority that the content process was set to.
   */
  parsePPMData(ppmDataString) {
    let [childIDStr, priority] = ppmDataString.split(":");
    return {
      childID: parseInt(childIDStr, 10),
      priority,
    };
  }

  /** nsIObserver **/
  observe(subject, topic, data) {
    if (topic != PRIORITY_SET_TOPIC) {
      Assert.ok(false, "TabPriorityWatcher is observing the wrong topic");
      return;
    }

    let { childID, priority } = this.parsePPMData(data);
    if (this.noChangeChildIDs.has(childID)) {
      this.noChangeChildIDs.set(childID, priority);
    }
    this.priorityMap.set(childID, priority);
  }
}

let gTabPriorityWatcher;

add_task(async function setup() {
  // We need to turn on testMode for the process priority manager in
  // order to receive the observer notifications that this test relies on.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.ipc.processPriorityManager.testMode", true],
      ["dom.ipc.processPriorityManager.enabled", true],
    ],
  });
  gTabPriorityWatcher = new TabPriorityWatcher(gBrowser);
});

registerCleanupFunction(() => {
  gTabPriorityWatcher.destroy();
  gTabPriorityWatcher = null;
});

/**
 * Utility function that switches the current tabbrowser from one
 * tab to another, and ensures that the tab that goes into the background
 * has (or reaches) a particular content process priority.
 *
 * It is expected that the fromTab and toTab belong to two separate content
 * processes.
 *
 * @param Object
 *   An object with the following properties:
 *
 *   fromTab (<tab>)
 *     The tab that will be switched from to the toTab. The fromTab
 *     is the one that will be going into the background.
 *
 *   toTab (<tab>)
 *     The tab that will be switched to from the fromTab. The toTab
 *     is presumed to start in the background, and will enter the
 *     foreground.
 *
 *   fromTabExpectedPriority (String)
 *     The priority that the content process for the fromTab is
 *     expected to be (or reach) after the tab goes into the background.
 *     This should be one of the PROCESS_PRIORITY_ strings defined at the
 *     top of the file.
 *
 * @return Promise
 * @resolves undefined
 *   Once the tab switch is complete, and the two content processes for the
 *   tabs have reached the expected priority levels.
 */
async function assertPriorityChangeOnBackground({
  fromTab,
  toTab,
  fromTabExpectedPriority,
}) {
  let fromBrowser = fromTab.linkedBrowser;
  let toBrowser = toTab.linkedBrowser;

  // If the tabs aren't running in separate processes, none of the
  // rest of this is going to work.
  Assert.notEqual(
    toBrowser.frameLoader.remoteTab.osPid,
    fromBrowser.frameLoader.remoteTab.osPid,
    "Tabs should be running in separate processes."
  );

  let fromPromise = gTabPriorityWatcher.waitForBrowserTreePriority(
    fromBrowser,
    fromTabExpectedPriority
  );
  let toPromise = gTabPriorityWatcher.waitForBrowserTreePriority(
    toBrowser,
    PROCESS_PRIORITY_FOREGROUND
  );

  await BrowserTestUtils.switchTab(gBrowser, toTab);
  await Promise.all([fromPromise, toPromise]);
}

/**
 * Test that if a normal tab goes into the background,
 * it has its process priority lowered to
 * PROCESS_PRIORITY_BACKGROUND.
 */
add_task(async function test_normal_background_tab() {
  let originalTab = gBrowser.selectedTab;

  await BrowserTestUtils.withNewTab(
    "http://example.com/browser/dom/ipc/tests/file_cross_frame.html",
    async browser => {
      let tab = gBrowser.getTabForBrowser(browser);
      await assertPriorityChangeOnBackground({
        fromTab: tab,
        toTab: originalTab,
        fromTabExpectedPriority: PROCESS_PRIORITY_BACKGROUND,
      });

      await assertPriorityChangeOnBackground({
        fromTab: originalTab,
        toTab: tab,
        fromTabExpectedPriority: PROCESS_PRIORITY_BACKGROUND,
      });
    }
  );
});

/**
 * If an iframe in a foreground tab is navigated to a new page for
 * a different site, then the process of the new iframe page should
 * have priority PROCESS_PRIORITY_FOREGROUND. Additionally, if Fission
 * is enabled, then the old iframe page's process's priority should be
 * lowered to PROCESS_PRIORITY_BACKGROUND.
 */
add_task(async function test_iframe_navigate() {
  // The URI we're going to navigate the iframe to.
  let iframeURI2 =
    "http://mochi.test:8888/browser/dom/ipc/tests/file_dummy.html";

  // Load this as a top level tab so that when we later load it as an iframe we
  // won't be creating a new process, so that we're testing the "load a new page"
  // part of prioritization and not the "initial process load" part.
  let newIFrameTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    iframeURI2
  );
  let newIFrameTabChildID = browsingContextChildID(
    gBrowser.selectedBrowser.browsingContext
  );

  Assert.equal(
    gTabPriorityWatcher.currentPriority(newIFrameTabChildID),
    PROCESS_PRIORITY_FOREGROUND,
    "Loading a new tab should make it prioritized"
  );

  await BrowserTestUtils.withNewTab(
    "http://example.com/browser/dom/ipc/tests/file_cross_frame.html",
    async browser => {
      Assert.equal(
        gTabPriorityWatcher.currentPriority(newIFrameTabChildID),
        PROCESS_PRIORITY_BACKGROUND,
        "Switching to a new tab should deprioritize the old one"
      );

      let iframe = browser.browsingContext.children[0];
      let iframeChildID1 = browsingContextChildID(iframe);

      // Do a cross-origin navigation in the iframe in the foreground tab.
      let loaded = BrowserTestUtils.browserLoaded(browser, true, iframeURI2);
      await SpecialPowers.spawn(iframe, [iframeURI2], async function(
        _iframeURI2
      ) {
        content.location = _iframeURI2;
      });
      await loaded;

      let iframeChildID2 = browsingContextChildID(iframe);
      let iframePriority1 = gTabPriorityWatcher.currentPriority(iframeChildID1);
      let iframePriority2 = gTabPriorityWatcher.currentPriority(iframeChildID2);

      if (SpecialPowers.useRemoteSubframes) {
        Assert.equal(
          newIFrameTabChildID,
          iframeChildID2,
          "The same site should get loaded into the same process"
        );
        Assert.notEqual(
          iframeChildID1,
          iframeChildID2,
          "Navigation should have switched processes"
        );
        Assert.equal(
          iframePriority1,
          PROCESS_PRIORITY_BACKGROUND,
          "The old iframe process should have been deprioritized"
        );
      } else {
        Assert.equal(
          iframeChildID1,
          iframeChildID2,
          "Navigation should not have switched processes"
        );
      }

      Assert.equal(
        iframePriority2,
        PROCESS_PRIORITY_FOREGROUND,
        "The new iframe process should be prioritized"
      );
    }
  );

  await BrowserTestUtils.removeTab(newIFrameTab);
});

/**
 * Test that a cross-group navigation properly preserves the process priority.
 * The goal of this test is to check that the code related to mPriorityActive in
 * CanonicalBrowsingContext::ReplacedBy works correctly, but in practice the
 * prioritization code in SetRenderLayers will also make this test pass, though
 * that prioritization happens slightly later.
 */
add_task(async function test_cross_group_navigate() {
  // This page is same-site with the page we're going to cross-group navigate to.
  let coopPage =
    "https://example.com/browser/dom/tests/browser/file_coop_coep.html";

  // Load it as a top level tab so that we don't accidentally get the initial
  // load prioritization.
  let backgroundTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    coopPage
  );
  let backgroundTabChildID = browsingContextChildID(
    gBrowser.selectedBrowser.browsingContext
  );

  Assert.equal(
    gTabPriorityWatcher.currentPriority(backgroundTabChildID),
    PROCESS_PRIORITY_FOREGROUND,
    "Loading a new tab should make it prioritized"
  );

  await BrowserTestUtils.withNewTab(
    "http://example.org/browser/dom/ipc/tests/file_cross_frame.html",
    async browser => {
      Assert.equal(
        gTabPriorityWatcher.currentPriority(backgroundTabChildID),
        PROCESS_PRIORITY_BACKGROUND,
        "Switching to a new tab should deprioritize the old one"
      );

      let dotOrgChildID = browsingContextChildID(browser.browsingContext);

      // Do a cross-group navigation.
      BrowserTestUtils.loadURI(browser, coopPage);
      await BrowserTestUtils.browserLoaded(browser);

      let coopChildID = browsingContextChildID(browser.browsingContext);
      let coopPriority = gTabPriorityWatcher.currentPriority(coopChildID);
      let dotOrgPriority = gTabPriorityWatcher.currentPriority(dotOrgChildID);

      Assert.equal(
        backgroundTabChildID,
        coopChildID,
        "The same site should get loaded into the same process"
      );
      Assert.notEqual(
        dotOrgChildID,
        coopChildID,
        "Navigation should have switched processes"
      );
      Assert.equal(
        dotOrgPriority,
        PROCESS_PRIORITY_BACKGROUND,
        "The old page process should have been deprioritized"
      );
      Assert.equal(
        coopPriority,
        PROCESS_PRIORITY_FOREGROUND,
        "The new page process should be prioritized"
      );
    }
  );

  await BrowserTestUtils.removeTab(backgroundTab);
});

/**
 * Test that if a tab with video goes into the background,
 * it has its process priority lowered to
 * PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE if it has no audio,
 * and that it has its priority remain at
 * PROCESS_PRIORITY_FOREGROUND if it does have audio.
 */
add_task(async function test_video_background_tab() {
  let originalTab = gBrowser.selectedTab;

  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    // Let's load up a video in the tab, but mute it, so that this tab should
    // reach PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE.
    await SpecialPowers.spawn(browser, [], async () => {
      let video = content.document.createElement("video");
      video.src = "http://mochi.test:8888/browser/dom/ipc/tests/short.mp4";
      video.muted = true;
      content.document.body.appendChild(video);
      // We'll loop the video to avoid it ending before the test is done.
      video.loop = true;
      await video.play();
    });

    let tab = gBrowser.getTabForBrowser(browser);

    // The tab with the muted video should reach
    // PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE when backgrounded.
    await assertPriorityChangeOnBackground({
      fromTab: tab,
      toTab: originalTab,
      fromTabExpectedPriority: PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE,
    });

    // Now switch back. The initial blank tab should reach
    // PROCESS_PRIORITY_BACKGROUND when backgrounded.
    await assertPriorityChangeOnBackground({
      fromTab: originalTab,
      toTab: tab,
      fromTabExpectedPriority: PROCESS_PRIORITY_BACKGROUND,
    });

    // Let's unmute the video now.
    await SpecialPowers.spawn(browser, [], async () => {
      let video = content.document.querySelector("video");
      video.muted = false;
    });

    // The tab with the unmuted video should stay at
    // PROCESS_PRIORITY_FOREGROUND when backgrounded.
    await assertPriorityChangeOnBackground({
      fromTab: tab,
      toTab: originalTab,
      fromTabExpectedPriority: PROCESS_PRIORITY_FOREGROUND,
    });

    // Now switch back. The initial blank tab should reach
    // PROCESS_PRIORITY_BACKGROUND when backgrounded.
    await assertPriorityChangeOnBackground({
      fromTab: originalTab,
      toTab: tab,
      fromTabExpectedPriority: PROCESS_PRIORITY_BACKGROUND,
    });
  });
});

/**
 * Test that if a tab with a playing <audio> element goes into
 * the background, the process priority does not change, unless
 * that audio is muted (in which case, it reaches
 * PROCESS_PRIORITY_BACKGROUND).
 */
add_task(async function test_audio_background_tab() {
  let originalTab = gBrowser.selectedTab;

  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    // Let's load up some audio in the tab, but mute it, so that this tab should
    // reach PROCESS_PRIORITY_BACKGROUND.
    await SpecialPowers.spawn(browser, [], async () => {
      let audio = content.document.createElement("audio");
      audio.src = "http://mochi.test:8888/browser/dom/ipc/tests/owl.mp3";
      audio.muted = true;
      content.document.body.appendChild(audio);
      // We'll loop the audio to avoid it ending before the test is done.
      audio.loop = true;
      await audio.play();
    });

    let tab = gBrowser.getTabForBrowser(browser);

    // The tab with the muted audio should reach
    // PROCESS_PRIORITY_BACKGROUND when backgrounded.
    await assertPriorityChangeOnBackground({
      fromTab: tab,
      toTab: originalTab,
      fromTabExpectedPriority: PROCESS_PRIORITY_BACKGROUND,
    });

    // Now switch back. The initial blank tab should reach
    // PROCESS_PRIORITY_BACKGROUND when backgrounded.
    await assertPriorityChangeOnBackground({
      fromTab: originalTab,
      toTab: tab,
      fromTabExpectedPriority: PROCESS_PRIORITY_BACKGROUND,
    });

    // Now unmute the audio. Unfortuntely, there's a bit of a race here,
    // since the wakelock on the audio element is released and then
    // re-acquired if the audio reaches its end and loops around. This
    // will cause an unexpected priority change on the content process.
    //
    // To avoid this race, we'll seek the audio back to the beginning,
    // and lower its playback rate to the minimum to increase the
    // likelihood that the check completes before the audio loops around.
    await SpecialPowers.spawn(browser, [], async () => {
      let audio = content.document.querySelector("audio");
      let seeked = ContentTaskUtils.waitForEvent(audio, "seeked");
      audio.muted = false;
      // 0.25 is the minimum playback rate that still keeps the audio audible.
      audio.playbackRate = 0.25;
      audio.currentTime = 0;
      await seeked;
    });

    // The tab with the unmuted audio should stay at
    // PROCESS_PRIORITY_FOREGROUND when backgrounded.
    await assertPriorityChangeOnBackground({
      fromTab: tab,
      toTab: originalTab,
      fromTabExpectedPriority: PROCESS_PRIORITY_FOREGROUND,
    });

    // Now switch back. The initial blank tab should reach
    // PROCESS_PRIORITY_BACKGROUND when backgrounded.
    await assertPriorityChangeOnBackground({
      fromTab: originalTab,
      toTab: tab,
      fromTabExpectedPriority: PROCESS_PRIORITY_BACKGROUND,
    });
  });
});

/**
 * Test that if a tab with a WebAudio playing goes into the background,
 * the process priority does not change, unless that WebAudio context is
 * suspended.
 */
add_task(async function test_web_audio_background_tab() {
  let originalTab = gBrowser.selectedTab;

  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
    // Let's synthesize a basic square wave as WebAudio.
    await SpecialPowers.spawn(browser, [], async () => {
      let audioCtx = new content.AudioContext();
      let oscillator = audioCtx.createOscillator();
      oscillator.type = "square";
      oscillator.frequency.setValueAtTime(440, audioCtx.currentTime);
      oscillator.connect(audioCtx.destination);
      oscillator.start();
      while (audioCtx.state != "running") {
        info(`wait until AudioContext starts running`);
        await new Promise(r => (audioCtx.onstatechange = r));
      }
      // we'll stash the AudioContext away so that it's easier to access
      // in the next SpecialPowers.spawn.
      content.audioCtx = audioCtx;
    });

    let tab = gBrowser.getTabForBrowser(browser);

    // The tab with the WebAudio should stay at
    // PROCESS_PRIORITY_FOREGROUND when backgrounded.
    await assertPriorityChangeOnBackground({
      fromTab: tab,
      toTab: originalTab,
      fromTabExpectedPriority: PROCESS_PRIORITY_FOREGROUND,
    });

    // Now switch back. The initial blank tab should reach
    // PROCESS_PRIORITY_BACKGROUND when backgrounded.
    await assertPriorityChangeOnBackground({
      fromTab: originalTab,
      toTab: tab,
      fromTabExpectedPriority: PROCESS_PRIORITY_BACKGROUND,
    });

    // Now suspend the WebAudio. This will cause it to stop
    // playing.
    await SpecialPowers.spawn(browser, [], async () => {
      content.audioCtx.suspend();
    });

    // The tab with the suspended WebAudio should reach
    // PROCESS_PRIORITY_BACKGROUND when backgrounded.
    await assertPriorityChangeOnBackground({
      fromTab: tab,
      toTab: originalTab,
      fromTabExpectedPriority: PROCESS_PRIORITY_BACKGROUND,
    });

    // Now switch back. The initial blank tab should reach
    // PROCESS_PRIORITY_BACKGROUND when backgrounded.
    await assertPriorityChangeOnBackground({
      fromTab: originalTab,
      toTab: tab,
      fromTabExpectedPriority: PROCESS_PRIORITY_BACKGROUND,
    });
  });
});
