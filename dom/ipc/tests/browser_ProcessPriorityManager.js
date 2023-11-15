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
    // Each value is an array of priorities we've seen the childID changed
    // to since it was added to the map. If the array is empty, there
    // have been no changes.
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
    this.noChangeChildIDs.set(childID, []);
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, WAIT_FOR_CHANGE_TIME_MS));
    let priorities = this.noChangeChildIDs.get(childID);
    Assert.deepEqual(
      priorities,
      [],
      `Should have seen no process priority changes for child ID ${childID}`
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
      this.noChangeChildIDs.get(childID).push(priority);
    }
    this.priorityMap.set(childID, priority);
  }
}

let gTabPriorityWatcher;

add_setup(async function () {
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
 * it has its process priority lowered to PROCESS_PRIORITY_BACKGROUND.
 * Additionally, test priorityHint flag sets the process priority
 * appropriately to PROCESS_PRIORITY_BACKGROUND and PROCESS_PRIORITY_FOREGROUND.
 */
add_task(async function test_normal_background_tab() {
  let originalTab = gBrowser.selectedTab;

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/dom/ipc/tests/file_cross_frame.html",
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

      let origtabID = browsingContextChildID(
        originalTab.linkedBrowser.browsingContext
      );

      Assert.equal(
        originalTab.linkedBrowser.frameLoader.remoteTab.priorityHint,
        false,
        "PriorityHint of the original tab should be false by default"
      );

      // Changing renderLayers doesn't change priority of the background tab.
      originalTab.linkedBrowser.preserveLayers(true);
      originalTab.linkedBrowser.renderLayers = true;
      await new Promise(resolve =>
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        setTimeout(resolve, WAIT_FOR_CHANGE_TIME_MS)
      );
      Assert.equal(
        gTabPriorityWatcher.currentPriority(origtabID),
        PROCESS_PRIORITY_BACKGROUND,
        "Tab didn't get prioritized only due to renderLayers"
      );

      // Test when priorityHint is true, the original tab priority
      // becomes PROCESS_PRIORITY_FOREGROUND.
      originalTab.linkedBrowser.frameLoader.remoteTab.priorityHint = true;
      Assert.equal(
        gTabPriorityWatcher.currentPriority(origtabID),
        PROCESS_PRIORITY_FOREGROUND,
        "Setting priorityHint to true should set the original tab to foreground priority"
      );

      // Test when priorityHint is false, the original tab priority
      // becomes PROCESS_PRIORITY_BACKGROUND.
      originalTab.linkedBrowser.frameLoader.remoteTab.priorityHint = false;
      await new Promise(resolve =>
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        setTimeout(resolve, WAIT_FOR_CHANGE_TIME_MS)
      );
      Assert.equal(
        gTabPriorityWatcher.currentPriority(origtabID),
        PROCESS_PRIORITY_BACKGROUND,
        "Setting priorityHint to false should set the original tab to background priority"
      );

      let tabID = browsingContextChildID(tab.linkedBrowser.browsingContext);

      // Test when priorityHint is true, the process priority of the
      // active tab remains PROCESS_PRIORITY_FOREGROUND.
      tab.linkedBrowser.frameLoader.remoteTab.priorityHint = true;
      Assert.equal(
        gTabPriorityWatcher.currentPriority(tabID),
        PROCESS_PRIORITY_FOREGROUND,
        "Setting priorityHint to true should maintain the new tab priority as foreground"
      );

      // Test when priorityHint is false, the process priority of the
      // active tab remains PROCESS_PRIORITY_FOREGROUND.
      tab.linkedBrowser.frameLoader.remoteTab.priorityHint = false;
      Assert.equal(
        gTabPriorityWatcher.currentPriority(tabID),
        PROCESS_PRIORITY_FOREGROUND,
        "Setting priorityHint to false should maintain the new tab priority as foreground"
      );

      originalTab.linkedBrowser.preserveLayers(false);
      originalTab.linkedBrowser.renderLayers = false;
    }
  );
});

// Load a simple page on the given host into a new tab.
async function loadKeepAliveTab(host) {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    host + "/browser/dom/ipc/tests/file_dummy.html"
  );
  let childID = browsingContextChildID(
    gBrowser.selectedBrowser.browsingContext
  );

  Assert.equal(
    gTabPriorityWatcher.currentPriority(childID),
    PROCESS_PRIORITY_FOREGROUND,
    "Loading a new tab should make it prioritized"
  );

  if (SpecialPowers.useRemoteSubframes) {
    // There must be only one process with a remote type for the tab we loaded
    // to ensure that when we load a new page into the iframe with that host
    // that it will end up in the same process as the initial tab.
    let remoteType = gBrowser.selectedBrowser.remoteType;
    await TestUtils.waitForCondition(() => {
      return (
        ChromeUtils.getAllDOMProcesses().filter(
          process => process.remoteType == remoteType
        ).length == 1
      );
    }, `Waiting for there to be only one process with remote type ${remoteType}`);
  }

  return { tab, childID };
}

const AUDIO_WAKELOCK_NAME = "audio-playing";
const VIDEO_WAKELOCK_NAME = "video-playing";

// This function was copied from toolkit/content/tests/browser/head.js
function wakeLockObserved(powerManager, observeTopic, checkFn) {
  return new Promise(resolve => {
    function wakeLockListener() {}
    wakeLockListener.prototype = {
      QueryInterface: ChromeUtils.generateQI(["nsIDOMMozWakeLockListener"]),
      callback(topic, state) {
        if (topic == observeTopic && checkFn(state)) {
          powerManager.removeWakeLockListener(wakeLockListener.prototype);
          resolve();
        }
      },
    };
    powerManager.addWakeLockListener(wakeLockListener.prototype);
  });
}

// This function was copied from toolkit/content/tests/browser/head.js
async function waitForExpectedWakeLockState(
  topic,
  { needLock, isForegroundLock }
) {
  const powerManagerService = Cc["@mozilla.org/power/powermanagerservice;1"];
  const powerManager = powerManagerService.getService(
    Ci.nsIPowerManagerService
  );
  const wakelockState = powerManager.getWakeLockState(topic);
  let expectedLockState = "unlocked";
  if (needLock) {
    expectedLockState = isForegroundLock
      ? "locked-foreground"
      : "locked-background";
  }
  if (wakelockState != expectedLockState) {
    info(`wait until wakelock becomes ${expectedLockState}`);
    await wakeLockObserved(
      powerManager,
      topic,
      state => state == expectedLockState
    );
  }
  is(
    powerManager.getWakeLockState(topic),
    expectedLockState,
    `the wakelock state for '${topic}' is equal to '${expectedLockState}'`
  );
}

/**
 * If an iframe in a foreground tab is navigated to a new page for
 * a different site, then the process of the new iframe page should
 * have priority PROCESS_PRIORITY_FOREGROUND. Additionally, if Fission
 * is enabled, then the old iframe page's process's priority should be
 * lowered to PROCESS_PRIORITY_BACKGROUND.
 */
add_task(async function test_iframe_navigate() {
  // This test (eventually) loads a page from the host topHost that has an
  // iframe from iframe1Host. It then navigates the iframe to iframe2Host.
  let topHost = "https://example.com";
  let iframe1Host = "https://example.org";
  let iframe2Host = "https://example.net";

  // Before we load the final test page into a tab, we need to load pages
  // from both iframe hosts into tabs. This is needed so that we are testing
  // the "load a new page" part of prioritization and not the "initial
  // process load" part. Additionally, it ensures that the process for the
  // initial iframe page doesn't shut down once we navigate away from it,
  // which will also affect its prioritization.
  let { tab: iframe1Tab, childID: iframe1TabChildID } = await loadKeepAliveTab(
    iframe1Host
  );
  let { tab: iframe2Tab, childID: iframe2TabChildID } = await loadKeepAliveTab(
    iframe2Host
  );

  await BrowserTestUtils.withNewTab(
    topHost + "/browser/dom/ipc/tests/file_cross_frame.html",
    async browser => {
      Assert.equal(
        gTabPriorityWatcher.currentPriority(iframe2TabChildID),
        PROCESS_PRIORITY_BACKGROUND,
        "Switching to another new tab should deprioritize the old one"
      );

      let topChildID = browsingContextChildID(browser.browsingContext);
      let iframe = browser.browsingContext.children[0];
      let iframe1ChildID = browsingContextChildID(iframe);

      Assert.equal(
        gTabPriorityWatcher.currentPriority(topChildID),
        PROCESS_PRIORITY_FOREGROUND,
        "The top level page in the new tab should be prioritized"
      );

      Assert.equal(
        gTabPriorityWatcher.currentPriority(iframe1ChildID),
        PROCESS_PRIORITY_FOREGROUND,
        "The iframe in the new tab should be prioritized"
      );

      if (SpecialPowers.useRemoteSubframes) {
        // Basic process uniqueness checks for the state after all three tabs
        // are initially loaded.
        Assert.notEqual(
          topChildID,
          iframe1ChildID,
          "file_cross_frame.html should be loaded into a different process " +
            "than its initial iframe"
        );

        Assert.notEqual(
          topChildID,
          iframe2TabChildID,
          "file_cross_frame.html should be loaded into a different process " +
            "than the tab containing iframe2Host"
        );

        Assert.notEqual(
          iframe1ChildID,
          iframe2TabChildID,
          "The initial iframe loaded by file_cross_frame.html should be " +
            "loaded into a different process than the tab containing " +
            "iframe2Host"
        );

        // Note: this assertion depends on our process selection logic.
        // Specifically, that we reuse an existing process for an iframe if
        // possible.
        Assert.equal(
          iframe1TabChildID,
          iframe1ChildID,
          "Both pages loaded in iframe1Host should be in the same process"
        );
      }

      // Do a cross-origin navigation in the iframe in the foreground tab.
      let iframe2URI = iframe2Host + "/browser/dom/ipc/tests/file_dummy.html";
      let loaded = BrowserTestUtils.browserLoaded(browser, true, iframe2URI);
      await SpecialPowers.spawn(
        iframe,
        [iframe2URI],
        async function (_iframe2URI) {
          content.location = _iframe2URI;
        }
      );
      await loaded;

      let iframe2ChildID = browsingContextChildID(iframe);
      let iframe1Priority = gTabPriorityWatcher.currentPriority(iframe1ChildID);
      let iframe2Priority = gTabPriorityWatcher.currentPriority(iframe2ChildID);

      if (SpecialPowers.useRemoteSubframes) {
        // Basic process uniqueness check for the state after navigating the
        // iframe. There's no need to check the top level pages because they
        // have not navigated.
        //
        // iframe1ChildID != iframe2ChildID is implied by:
        //   iframe1ChildID != iframe2TabChildID
        //   iframe2TabChildID == iframe2ChildID
        //
        // iframe2ChildID != topChildID is implied by:
        //   topChildID != iframe2TabChildID
        //   iframe2TabChildID == iframe2ChildID

        // Note: this assertion depends on our process selection logic.
        // Specifically, that we reuse an existing process for an iframe if
        // possible. If that changes, this test may need to be carefully
        // rewritten, as the whole point of the test is to check what happens
        // with the priority manager when an iframe shares a process with
        // a page in another tab.
        Assert.equal(
          iframe2TabChildID,
          iframe2ChildID,
          "Both pages loaded in iframe2Host should be in the same process"
        );

        // Now that we've established the relationship between the various
        // processes, we can finally check that the priority manager is doing
        // the right thing.
        Assert.equal(
          iframe1Priority,
          PROCESS_PRIORITY_BACKGROUND,
          "The old iframe process should have been deprioritized"
        );
      } else {
        Assert.equal(
          iframe1ChildID,
          iframe2ChildID,
          "Navigation should not have switched processes"
        );
      }

      Assert.equal(
        iframe2Priority,
        PROCESS_PRIORITY_FOREGROUND,
        "The new iframe process should be prioritized"
      );
    }
  );

  await BrowserTestUtils.removeTab(iframe2Tab);
  await BrowserTestUtils.removeTab(iframe1Tab);
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
    "https://example.org/browser/dom/ipc/tests/file_cross_frame.html",
    async browser => {
      Assert.equal(
        gTabPriorityWatcher.currentPriority(backgroundTabChildID),
        PROCESS_PRIORITY_BACKGROUND,
        "Switching to a new tab should deprioritize the old one"
      );

      let dotOrgChildID = browsingContextChildID(browser.browsingContext);

      // Do a cross-group navigation.
      BrowserTestUtils.startLoadingURIString(browser, coopPage);
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

  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    // Let's load up a video in the tab, but mute it, so that this tab should
    // reach PROCESS_PRIORITY_BACKGROUND_PERCEIVABLE. We need to wait for the
    // wakelock changes from the unmuting to get back up to the parent.
    await SpecialPowers.spawn(browser, [], async () => {
      let video = content.document.createElement("video");
      video.src = "https://example.net/browser/dom/ipc/tests/short.mp4";
      video.muted = true;
      content.document.body.appendChild(video);
      // We'll loop the video to avoid it ending before the test is done.
      video.loop = true;
      await video.play();
    });
    await Promise.all([
      waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
        needLock: false,
      }),
      waitForExpectedWakeLockState(VIDEO_WAKELOCK_NAME, {
        needLock: true,
        isForegroundLock: true,
      }),
    ]);

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

    // Let's unmute the video now. We need to wait for the wakelock change from
    // the unmuting to get back up to the parent.
    await Promise.all([
      waitForExpectedWakeLockState(AUDIO_WAKELOCK_NAME, {
        needLock: true,
        isForegroundLock: true,
      }),
      SpecialPowers.spawn(browser, [], async () => {
        let video = content.document.querySelector("video");
        video.muted = false;
      }),
    ]);

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

  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
    // Let's load up some audio in the tab, but mute it, so that this tab should
    // reach PROCESS_PRIORITY_BACKGROUND.
    await SpecialPowers.spawn(browser, [], async () => {
      let audio = content.document.createElement("audio");
      audio.src = "https://example.net/browser/dom/ipc/tests/owl.mp3";
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

  await BrowserTestUtils.withNewTab("https://example.com", async browser => {
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

/**
 * Test that foreground tab's process priority isn't changed when going back to
 * a bfcached session history entry.
 */
add_task(async function test_audio_background_tab() {
  let page1 = "https://example.com";
  let page2 = page1 + "/?2";

  await BrowserTestUtils.withNewTab(page1, async browser => {
    let childID = browsingContextChildID(browser.browsingContext);
    Assert.equal(
      gTabPriorityWatcher.currentPriority(childID),
      PROCESS_PRIORITY_FOREGROUND,
      "Loading a new tab should make it prioritized."
    );
    let loaded = BrowserTestUtils.browserLoaded(browser, false, page2);
    BrowserTestUtils.startLoadingURIString(browser, page2);
    await loaded;

    childID = browsingContextChildID(browser.browsingContext);
    Assert.equal(
      gTabPriorityWatcher.currentPriority(childID),
      PROCESS_PRIORITY_FOREGROUND,
      "Loading a new page should keep the tab prioritized."
    );

    let pageShowPromise = BrowserTestUtils.waitForContentEvent(
      browser,
      "pageshow"
    );
    browser.goBack();
    await pageShowPromise;

    childID = browsingContextChildID(browser.browsingContext);
    Assert.equal(
      gTabPriorityWatcher.currentPriority(childID),
      PROCESS_PRIORITY_FOREGROUND,
      "Loading a page from the bfcache should keep the tab prioritized."
    );
  });
});
