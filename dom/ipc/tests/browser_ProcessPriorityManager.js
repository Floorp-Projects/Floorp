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

    this.priorityMap = new WeakMap();
    this.priorityMap.set(
      this.tabbrowser.selectedBrowser,
      PROCESS_PRIORITY_FOREGROUND
    );
    this.noChangeBrowsers = new WeakMap();
    Services.obs.addObserver(this, PRIORITY_SET_TOPIC);
  }

  /**
   * Cleans up lingering references for an instance of
   * TabPriorityWatcher to avoid leaks. This should be called when
   * finishing the test.
   */
  destroy() {
    Services.obs.removeObserver(this, PRIORITY_SET_TOPIC);
    this.window = null;
  }

  /**
   * Returns a Promise that resolves when a particular <browser>
   * has its content process reach a particular priority. Will
   * eventually time out if that priority is never reached.
   *
   * @param browser (<browser>)
   *   The <browser> that we expect to change priority.
   * @param expectedPriority (String)
   *   One of the PROCESS_PRIORITY_ constants defined at the
   *   top of this file.
   * @return Promise
   * @resolves undefined
   *   Once the browser reaches the expected priority.
   */
  async waitForPriorityChange(browser, expectedPriority) {
    return TestUtils.waitForCondition(() => {
      let currentPriority = this.priorityMap.get(browser);
      if (currentPriority == expectedPriority) {
        Assert.ok(
          true,
          `Browser at ${browser.currentURI.spec} reached expected ` +
            `priority: ${currentPriority}`
        );
        return true;
      }
      return false;
    }, `Waiting for browser at ${browser.currentURI.spec} to reach priority ` + expectedPriority);
  }

  /**
   * Returns a Promise that resolves after a duration of
   * WAIT_FOR_CHANGE_TIME_MS. During that time, if the passed browser
   * changes priority, a test failure will be registered.
   *
   * @param browser (<browser>)
   *   The <browser> that we expect to change priority.
   * @return Promise
   * @resolves undefined
   *   Once the WAIT_FOR_CHANGE_TIME_MS duration has passed.
   */
  async ensureNoPriorityChange(browser) {
    this.noChangeBrowsers.set(browser, null);
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, WAIT_FOR_CHANGE_TIME_MS));
    let priority = this.noChangeBrowsers.get(browser);
    Assert.equal(
      priority,
      null,
      `Should have seen no process priority change for a browser at ${browser.currentURI.spec}`
    );
    this.noChangeBrowsers.delete(browser);
  }

  /**
   * Makes sure that a particular foreground browser has been
   * registered in the priority map. This is needed because browsers are
   * only registered when their priorities change - and if a browser's
   * priority never changes during a test, then they wouldn't be registered.
   *
   * The passed browser must be a foreground browser, since it's assumed that
   * the associated content process is running with foreground priority.
   *
   * @param browser (browser)
   *   A _foreground_ browser.
   */
  ensureForegroundRegistered(browser) {
    if (!this.priorityMap.has(browser)) {
      this.priorityMap.set(browser, PROCESS_PRIORITY_FOREGROUND);
    }
  }

  /**
   * Synchronously returns the priority of a particular browser's
   * content process.
   *
   * @param browser (browser)
   *   The browser to get the content process priority for.
   * @return String
   *   The priority that the browser's content process is at.
   */
  currentPriority(browser) {
    return this.priorityMap.get(browser);
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
    for (let browser of this.tabbrowser.browsers) {
      if (browser.frameLoader.childID == childID) {
        info(
          `Browser at: ${browser.currentURI.spec} transitioning to ${priority}`
        );
        if (this.noChangeBrowsers.has(browser)) {
          this.noChangeBrowsers.set(browser, priority);
        }
        this.priorityMap.set(browser, priority);
      }
    }
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

  gTabPriorityWatcher.ensureForegroundRegistered(fromBrowser);

  let fromPromise;
  if (
    gTabPriorityWatcher.currentPriority(fromBrowser) == fromTabExpectedPriority
  ) {
    fromPromise = gTabPriorityWatcher.ensureNoPriorityChange(fromBrowser);
  } else {
    fromPromise = gTabPriorityWatcher.waitForPriorityChange(
      fromBrowser,
      fromTabExpectedPriority
    );
  }

  let toPromise;
  if (
    gTabPriorityWatcher.currentPriority(toBrowser) ==
    PROCESS_PRIORITY_FOREGROUND
  ) {
    toPromise = gTabPriorityWatcher.ensureNoPriorityChange(toBrowser);
  } else {
    toPromise = gTabPriorityWatcher.waitForPriorityChange(
      toBrowser,
      PROCESS_PRIORITY_FOREGROUND
    );
  }

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

  await BrowserTestUtils.withNewTab("http://example.com", async browser => {
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
  });
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
add_task(async function test_audio_background_tab() {
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
