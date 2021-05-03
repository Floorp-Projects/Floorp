/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * TabUnloader is used to discard tabs when memory or resource constraints
 * are reached. The discarded tabs are determined using a heuristic that
 * accounts for when the tab was last used, how many resources the tab uses,
 * and whether the tab is likely to affect the user if it is closed.
 */
var EXPORTED_SYMBOLS = ["TabUnloader"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "webrtcUI",
  "resource:///modules/webrtcUI.jsm"
);

// If there are only this many or fewer tabs open, just sort by weight, and close
// the lowest tab. Otherwise, do a more intensive compuation that determines the
// tabs to close based on memory and process use.
const MIN_TABS_COUNT = 10;

// Weight for non-discardable tabs.
const NEVER_DISCARD = 100000;

let criteriaTypes = [
  ["isNonDiscardable", NEVER_DISCARD],
  ["isLoading", 8],
  ["usingPictureInPicture", 4],
  ["playingMedia", 3],
  ["usingWebRTC", 3],
  ["isPinned", 2],
];

// Indicies into the criteriaTypes lists.
let CRITERIA_METHOD = 0;
let CRITERIA_WEIGHT = 1;

/**
 * This is an object that supplies methods that determine details about
 * each tab. This default object is used if another one is not passed
 * to the tab unloader functions. This allows tests to override the methods
 * with tab specific data rather than creating test tabs.
 */
let DefaultTabUnloaderMethods = {
  isNonDiscardable(tab, weight) {
    if (tab.selected) {
      return weight;
    }

    return !tab.linkedBrowser.isConnected ? -1 : 0;
  },

  isPinned(tab, weight) {
    return tab.pinned ? weight : 0;
  },

  isLoading(tab, weight) {
    return 0;
  },

  usingPictureInPicture(tab, weight) {
    // This has higher weight even when paused.
    return tab.pictureinpicture ? weight : 0;
  },

  playingMedia(tab, weight) {
    return tab.soundPlaying ? weight : 0;
  },

  usingWebRTC(tab, weight) {
    return webrtcUI.browserHasStreams(tab.linkedBrowser) ? weight : 0;
  },

  getMinTabCount() {
    return MIN_TABS_COUNT;
  },

  *iterateTabs() {
    for (let win of Services.wm.getEnumerator("navigator:browser")) {
      for (let tab of win.gBrowser.tabs) {
        yield { tab, gBrowser: win.gBrowser };
      }
    }
  },

  *iterateBrowsingContexts(bc) {
    yield bc;
    for (let childBC of bc.children) {
      yield* this.iterateBrowsingContexts(childBC);
    }
  },

  *iterateProcesses(tab) {
    let bc = tab?.linkedBrowser?.browsingContext;
    if (!bc) {
      return;
    }

    const iter = this.iterateBrowsingContexts(bc);
    for (let childBC of iter) {
      if (childBC?.currentWindowGlobal) {
        yield childBC.currentWindowGlobal.osPid;
      }
    }
  },

  /**
   * Add the amount of memory used by each process to the process map.
   *
   * @param tabs array of tabs, used only by unit tests
   * @param map of processes returned by getAllProcesses.
   */
  async calculateMemoryUsage(tabs, processMap) {
    let parentProcessInfo = await ChromeUtils.requestProcInfo();
    let childProcessInfoList = parentProcessInfo.children;
    for (let childProcInfo of childProcessInfoList) {
      let processInfo = processMap.get(childProcInfo.pid);
      if (!processInfo) {
        processInfo = { count: 0, topCount: 0 };
        processMap.set(childProcInfo.pid, processInfo);
      }
      processInfo.memory = childProcInfo.residentUniqueSize;
    }
  },
};

/**
 * This module is responsible for detecting low-memory scenarios and unloading
 * tabs in response to them.
 */

var TabUnloader = {
  /**
   * Initialize low-memory detection and tab auto-unloading.
   */
  init() {
    if (Services.prefs.getBoolPref("browser.tabs.unloadOnLowMemory", true)) {
      Services.obs.addObserver(this, "memory-pressure", /* ownsWeak */ true);
    }
  },

  observe(subject, topic, data) {
    if (topic == "memory-pressure" && data != "heap-minimize") {
      this.unloadLeastRecentlyUsedTab();
    }
  },

  /**
   * Get a list of tabs that can be discarded. This list includes all tabs in
   * all windows and is sorted based on a weighting described below.
   *
   * @param tabMethods an helper object with methods called by this algorithm.
   *
   * The algorithm used is:
   *   1. Sort all of the tabs by a base weight. Tabs with a higher weight, such as
   *      those that are pinned or playing audio, will appear at the end. When two
   *      tabs have the same weight, sort by the order in which they were last.
   *      recently accessed Tabs that have a weight of NEVER_DISCARD are included in
   *       the list, but will not be discarded.
   *   2. Exclude the last X tabs, where X is the value returned by getMinTabCount().
   *      These tabs are considered to have been recently accessed and are not further
   *      reweighted. This also saves time when there are less than X tabs open.
   *   3. Calculate the amount of processes that are used only by each tab, as the
   *      resources used by these proceses can be freed up if the tab is closed. Sort
   *      the tabs by the number of unique processes used and add a reweighting factor
   *      based on this.
   *   4. Futher reweight based on an approximation of the amount of memory that each
   *      tab uses.
   *   5. Combine these weights to produce a final tab discard order, and discard the
   *      first tab. If this fails, then discard the next tab in the list until no more
   *      non-discardable tabs are found.
   *
   * The tabMethods are used so that unit tests can use false tab objects and
   * override their behaviour.
   */
  async getSortedTabs(tabMethods = DefaultTabUnloaderMethods) {
    let tabs = [];

    let lowestWeight = 1000;
    for (let tab of tabMethods.iterateTabs()) {
      let weight = determineTabBaseWeight(tab, tabMethods);

      // Don't add tabs that have a weight of -1.
      if (weight != -1) {
        tab.weight = weight;
        tabs.push(tab);
        if (weight < lowestWeight) {
          lowestWeight = weight;
        }
      }
    }

    tabs = tabs.sort((a, b) => {
      if (a.weight != b.weight) {
        return a.weight - b.weight;
      }

      return a.tab.lastAccessed - b.tab.lastAccessed;
    });

    // If the lowest priority tab is not discardable, don't discard any tabs.
    if (!tabs.length || tabs[0].weight == NEVER_DISCARD) {
      return [];
    }

    // Determine the lowest weight that the tabs have. The tabs with the
    // lowest weight (should be most non-selected tabs) will be additionally
    // weighted by the number of processes and memory that they use.
    let higherWeightedCount = 0;
    for (let idx = 0; idx < tabs.length; idx++) {
      if (tabs[idx].weight != lowestWeight) {
        higherWeightedCount = tabs.length - idx;
        break;
      }
    }

    // Don't continue to reweight the last few tabs, the number of which is
    // determined by getMinTabCount. This prevents extra work when there are
    // only a few tabs, or for the last few tabs that have likely been used
    // recently.
    let minCount = tabMethods.getMinTabCount();
    if (higherWeightedCount < minCount) {
      higherWeightedCount = minCount;
    }

    if (higherWeightedCount < tabs.length) {
      let processMap = getAllProcesses(tabs, tabMethods);

      let higherWeightedTabs = tabs.splice(-higherWeightedCount);

      await adjustForResourceUse(tabs, processMap, tabMethods);
      tabs = tabs.concat(higherWeightedTabs);
    }

    return tabs;
  },

  /**
   * Select and discard one tab.
   */
  async unloadLeastRecentlyUsedTab() {
    let sortedTabs = await this.getSortedTabs();

    for (let tabInfo of sortedTabs) {
      if (tabInfo.weight == NEVER_DISCARD) {
        return;
      }

      if (tabInfo.gBrowser.discardBrowser(tabInfo.tab)) {
        return;
      }
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),
};

/** Determine the base weight of the tab without accounting for
 *  resource use
 * @param tab tab to use
 * @returns the tab's base weight
 */
function determineTabBaseWeight(tab, tabMethods) {
  let totalWeight = 0;

  for (let criteriaType of criteriaTypes) {
    let weight = tabMethods[criteriaType[CRITERIA_METHOD]](
      tab.tab,
      criteriaType[CRITERIA_WEIGHT]
    );

    // If a criteria returns -1, then never discard this tab.
    if (weight == -1) {
      return -1;
    }

    totalWeight += weight;
  }

  return totalWeight;
}

/**
 * Constuct a map of the processes that are used by the supplied tabs.
 * The map will map process ids to an object with two properties:
 *   count - the number of tabs or subframes that use this process
 *   topCount - the number of top-level tabs that use this process
 *
 * @param tabs array of tabs
 * @param tabMethods an helper object with methods called by this algorithm.
 * @returns process map
 */
function getAllProcesses(tabs, tabMethods) {
  // Determine the number of tabs that reference each process. This
  // is stored in the map 'processMap' where the key is the process
  // and the value is that number of browsing contexts that use that
  // process.
  // XXXndeakin this should be unique processes per tab, in the case multiple
  // subframes use the same process?

  let processMap = new Map();

  for (let tab of tabs) {
    let topLevel = true;
    for (let pid of tabMethods.iterateProcesses(tab.tab)) {
      let processInfo = processMap.get(pid);
      if (processInfo) {
        processInfo.count++;
      } else {
        processInfo = { count: 1, topCount: 0 };
        processMap.set(pid, processInfo);
      }

      if (topLevel) {
        topLevel = false;
        processInfo.topCount = processInfo.topCount
          ? processInfo.topCount + 1
          : 1;
      }
    }
  }

  return processMap;
}

/**
 * Adjust the tab info and reweight the tabs based on the process and memory
 * use that is used, as described by getSortedTabs

 * @param tabs array of tabs
 * @param processMap map of processes returned by getAllProcesses
 * @param tabMethods an helper object with methods called by this algorithm.
 */
async function adjustForResourceUse(tabs, processMap, tabMethods) {
  await tabMethods.calculateMemoryUsage(tabs, processMap);

  let sortWeight = 0;
  for (let tab of tabs) {
    tab.sortWeight = ++sortWeight;

    let topLevel = true;
    let uniqueCount = 0;
    let totalMemory = 0;
    for (let pid of tabMethods.iterateProcesses(tab.tab)) {
      let processInfo = processMap.get(pid);
      let { count, topCount, memory } = processInfo;
      if (count == 1) {
        uniqueCount++;
      }

      // Guess how much memory the frame might be using using by dividing
      // the total memory used by a process by the number of tabs and
      // frames that are using that process. Assume that any subframes take up
      // only half as much memory as a process loaded in a top level tab.
      // So for example, if a process is used in four top level tabs and two
      // subframes, the top level tabs share 80% of the memory and the subframes
      // use 20% of the memory.
      let perFrameMemory = memory / (topCount * 2 + (count - topCount));
      if (topLevel) {
        topLevel = false;
        perFrameMemory *= 2;
      }
      totalMemory += perFrameMemory;
    }

    tab.uniqueCount = uniqueCount;
    tab.memory = totalMemory;
  }

  tabs.sort((a, b) => {
    return b.uniqueCount - a.uniqueCount;
  });
  sortWeight = 0;
  for (let tab of tabs) {
    tab.sortWeight += ++sortWeight;
    if (tab.uniqueCount > 1) {
      // If the tab has a number of processes that are only used by this tab,
      // subtract off an additional amount to the sorting weight value. That
      // way, tabs that use lots of processes are more likely to be discarded.
      tab.sortWeight -= tab.uniqueCount - 1;
    }
  }

  tabs.sort((a, b) => {
    return b.memory - a.memory;
  });
  sortWeight = 0;
  for (let tab of tabs) {
    tab.sortWeight += ++sortWeight;
  }

  tabs.sort((a, b) => {
    if (a.sortWeight != b.sortWeight) {
      return a.sortWeight - b.sortWeight;
    }
    return a.tab.lastAccessed - b.tab.lastAccessed;
  });
}
