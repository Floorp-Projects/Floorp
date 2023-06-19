/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { TabUnloader } = ChromeUtils.importESModule(
  "resource:///modules/TabUnloader.sys.mjs"
);

let TestTabUnloaderMethods = {
  isNonDiscardable(tab, weight) {
    return /\bselected\b/.test(tab.keywords) ? weight : 0;
  },

  isParentProcess(tab, weight) {
    return /\bparent\b/.test(tab.keywords) ? weight : 0;
  },

  isPinned(tab, weight) {
    return /\bpinned\b/.test(tab.keywords) ? weight : 0;
  },

  isLoading(tab, weight) {
    return /\bloading\b/.test(tab.keywords) ? weight : 0;
  },

  usingPictureInPicture(tab, weight) {
    return /\bpictureinpicture\b/.test(tab.keywords) ? weight : 0;
  },

  playingMedia(tab, weight) {
    return /\bmedia\b/.test(tab.keywords) ? weight : 0;
  },

  usingWebRTC(tab, weight) {
    return /\bwebrtc\b/.test(tab.keywords) ? weight : 0;
  },

  isPrivate(tab, weight) {
    return /\bprivate\b/.test(tab.keywords) ? weight : 0;
  },

  getMinTabCount() {
    // Use a low number for testing.
    return 3;
  },

  getNow() {
    return 100;
  },

  *iterateProcesses(tab) {
    for (let process of tab.process.split(",")) {
      yield Number(process);
    }
  },

  async calculateMemoryUsage(processMap, tabs) {
    let memory = tabs[0].memory;
    for (let pid of processMap.keys()) {
      processMap.get(pid).memory = memory ? memory[pid - 1] : 1;
    }
  },
};

let unloadTests = [
  // Each item in the array represents one test. The test is a subarray
  // containing an element per tab. This is a string of keywords that
  // identify which criteria apply. The first part of the string may contain
  // a number that represents the last visit time, where higher numbers
  // are later. The last element in the subarray is special and identifies
  // the expected order of the tabs sorted by weight. The first tab in
  // this list is the one that is expected to selected to be discarded.
  { tabs: ["1 selected", "2", "3"], result: "1,2,0" },
  { tabs: ["1", "2 selected", "3"], result: "0,2,1" },
  { tabs: ["1 selected", "2", "3"], process: ["1", "2", "3"], result: "1,2,0" },
  {
    tabs: ["1 selected", "2 selected", "3 selected"],
    process: ["1", "2", "3"],
    result: "0,1,2",
  },
  {
    tabs: ["1 selected", "2", "3"],
    process: ["1,2,3", "2", "3"],
    result: "1,2,0",
  },
  {
    tabs: ["9", "8", "6", "5 selected", "2", "3", "4", "1"],
    result: "7,4,5,6,2,1,0,3",
  },
  {
    tabs: ["9", "8 pinned", "6", "5 selected", "2", "3 pinned", "4", "1"],
    result: "7,4,6,2,0,5,1,3",
  },
  {
    tabs: [
      "9",
      "8 pinned",
      "6",
      "5 selected pinned",
      "2",
      "3 pinned",
      "4",
      "1",
    ],
    result: "7,4,6,2,0,5,1,3",
  },
  {
    tabs: [
      "9",
      "8 pinned",
      "6",
      "5 selected pinned",
      "2",
      "3 selected pinned",
      "4",
      "1",
    ],
    result: "7,4,6,2,0,1,5,3",
  },
  {
    tabs: ["1", "2 selected", "3", "4 media", "5", "6"],
    result: "0,2,4,5,1,3",
  },
  {
    tabs: ["1 media", "2 selected media", "3", "4 media", "5", "6"],
    result: "2,4,5,0,3,1",
  },
  {
    tabs: ["1 media", "2 media pinned", "3", "4 media", "5 pinned", "6"],
    result: "2,5,4,0,3,1",
  },
  {
    tabs: [
      "1 media",
      "2 media pinned",
      "3",
      "4 media",
      "5 media pinned",
      "6 selected",
    ],
    result: "2,0,3,5,1,4",
  },
  {
    tabs: [
      "10 selected",
      "20 private",
      "30 webrtc",
      "40 pictureinpicture",
      "50 loading pinned",
      "60",
    ],
    result: "5,4,0,1,2,3",
  },
  {
    // Since TestTabUnloaderMethods.getNow() returns 100 and the test
    // passes minInactiveDuration = 0 to TabUnloader.getSortedTabs(),
    // tab 200 and 300 are excluded from the result.
    tabs: ["300", "10", "50", "100", "200"],
    result: "1,2,3",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6"],
    process: ["1", "2", "1", "1", "1", "1"],
    result: "1,0,2,3,4,5",
  },
  {
    tabs: ["1", "2 selected", "3", "4", "5", "6"],
    process: ["1", "2", "1", "1", "1", "1"],
    result: "0,2,3,4,5,1",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6"],
    process: ["1", "2", "2", "1", "1", "1"],
    result: "0,1,2,3,4,5",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6"],
    process: ["1", "2", "3", "1", "1", "1"],
    result: "1,0,2,3,4,5",
  },
  {
    tabs: ["1", "2 media", "3", "4", "5", "6"],
    process: ["1", "2", "3", "1", "1", "1"],
    result: "2,0,3,4,5,1",
  },
  {
    tabs: ["1", "2 media", "3", "4", "5", "6"],
    process: ["1", "2", "3", "1", "1,2,3", "1"],
    result: "0,2,3,4,5,1",
  },
  {
    tabs: ["1", "2 media", "3", "4", "5", "6"],
    process: ["1", "2", "3", "1", "1,4,5", "1"],
    result: "2,0,3,4,5,1",
  },
  {
    tabs: ["1", "2 media", "3 media", "4", "5 media", "6"],
    process: ["1", "2", "3", "1", "1,4,5", "1"],
    result: "0,3,5,1,2,4",
  },
  {
    tabs: ["1", "2 media", "3 media", "4", "5 media", "6"],
    process: ["1", "1", "3", "1", "1,4,5", "1"],
    result: "0,3,5,1,2,4",
  },
  {
    tabs: ["1", "2 media", "3 media", "4", "5 media", "6"],
    process: ["1", "2", "3", "4", "1,4,5", "5"],
    result: "0,3,5,1,2,4",
  },
  {
    tabs: ["1", "2 media", "3 media", "4", "5 media", "6"],
    process: ["1", "1", "3", "4", "1,4,5", "5"],
    result: "0,3,5,1,2,4",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6"],
    process: ["1", "1", "1", "2", "1,3,4,5,6,7,8", "1"],
    result: "0,1,2,3,4,5",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6", "7", "8"],
    process: ["1", "1", "1", "2", "1,3,4,5,6,7,8", "1", "1", "1"],
    result: "4,0,3,1,2,5,6,7",
  },
  {
    tabs: ["1", "2", "3", "4", "5 selected", "6"],
    process: ["1", "1", "1", "2", "1,3,4,5,6,7,8", "1"],
    result: "0,1,2,3,5,4",
  },
  {
    tabs: ["1", "2", "3", "4", "5 media", "6"],
    process: ["1", "1", "1", "2", "1,3,4,5,6,7,8", "1"],
    result: "0,1,2,3,5,4",
  },
  {
    tabs: ["1", "2", "3", "4", "5 media", "6", "7", "8"],
    process: ["1", "1", "1", "2", "1,3,4,5,6,7,8", "1", "1", "1"],
    result: "0,3,1,2,5,6,7,4",
  },
  {
    tabs: ["1", "2", "3", "4", "5 media", "6", "7", "8"],
    process: ["1", "1,3,4,5,6,7,8", "1", "1", "1", "1", "1", "1"],
    result: "1,0,2,3,5,6,7,4",
  },
  {
    tabs: ["1", "2", "3", "4", "5 media", "6", "7", "8"],
    process: ["1", "1", "1,3,4,5,6,7,8", "1", "1", "1", "1", "1"],
    result: "2,0,1,3,5,6,7,4",
  },
  {
    tabs: ["1", "2", "3", "4", "5 media", "6", "7", "8"],
    process: ["1", "1", "1,1,1,1,1,1,1", "1", "1", "1", "1,1,1,1,1", "1"],
    result: "0,1,2,3,5,6,7,4",
  },
  {
    tabs: ["1", "2", "3", "4", "5 media", "6", "7", "8"],
    process: ["1", "1", "1,2,3,4,5", "1", "1", "1", "1,2,3,4,5", "1"],
    result: "0,1,2,3,5,6,7,4",
  },
  {
    tabs: ["1", "2", "3", "4", "5 media", "6", "7", "8"],
    process: ["1", "1", "1,6", "1", "1", "1", "1,2,3,4,5", "1"],
    result: "0,2,1,3,5,6,7,4",
  },
  {
    tabs: ["1", "2", "3", "4", "5 media", "6", "7", "8"],
    process: ["1", "1", "1,6", "1,7", "1,8", "1,9", "1,2,3,4,5", "1"],
    result: "2,3,0,5,1,6,7,4",
  },
  {
    tabs: ["1", "2", "3", "4", "5 media", "6", "7", "8"],
    process: ["1,10,11", "1", "1,2", "1,7", "1,8", "1,9", "1,2,3,4,5", "1"],
    result: "0,3,1,5,2,6,7,4",
  },
  {
    tabs: [
      "1 media",
      "2 media",
      "3 media",
      "4 media",
      "5 media",
      "6",
      "7",
      "8",
    ],
    process: ["1,10,11", "1", "1,2", "1,7", "1,8", "1,9", "1,2,3,4,5", "1"],
    result: "6,5,7,0,1,2,3,4",
  },
  {
    tabs: ["1", "2", "3"],
    process: ["1", "2", "3"],
    memory: ["100", "200", "300"],
    result: "0,1,2",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10"],
    process: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10"],
    memory: [
      "100",
      "200",
      "300",
      "400",
      "500",
      "600",
      "700",
      "800",
      "900",
      "1000",
    ],
    result: "0,1,2,3,4,5,6,7,8,9",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10"],
    process: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10"],
    memory: [
      "100",
      "900",
      "300",
      "500",
      "400",
      "700",
      "600",
      "1000",
      "200",
      "200",
    ],
    result: "1,0,2,3,5,4,6,7,8,9",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10"],
    process: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "10"],
    memory: [
      "1000",
      "900",
      "300",
      "500",
      "400",
      "1000",
      "600",
      "1000",
      "200",
      "200",
    ],
    result: "0,1,2,3,5,4,6,7,8,9",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6"],
    process: ["1", "2,7", "3", "4", "5", "6"],
    memory: ["100", "200", "300", "400", "500", "600", "700"],
    result: "1,0,2,3,4,5",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6", "7", "8"],
    process: ["1,6", "2,7", "3,8", "4,1,2", "5", "6", "7", "8"],
    memory: ["100", "200", "300", "400", "500", "600", "700", "800"],
    result: "2,3,0,1,4,5,6,7",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6", "7", "8"],
    process: ["1", "1", "1", "2", "1", "1", "1", "1"],
    memory: ["700", "1000"],
    result: "0,3,1,2,4,5,6,7",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6", "7", "8"],
    process: ["1", "1", "1", "1", "2,1", "2,1", "3", "3"],
    memory: ["1000", "2000", "3000"],
    result: "0,1,2,4,3,5,6,7",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6", "7", "8"],
    process: ["2", "2", "2", "2", "2,1", "2,1", "3", "3"],
    memory: ["1000", "600", "1000"],
    result: "0,1,2,4,3,5,6,7",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6", "7", "8"],
    process: ["1", "1", "1", "2", "2,1,1,1", "2,1", "3", "3"],
    memory: ["1000", "1800", "1000"],
    result: "0,1,3,2,4,5,6,7",
  },
  {
    tabs: ["1", "2", "3", "4", "5", "6", "7", "8"],
    process: ["1", "1", "1", "2", "2,1,1,1", "2,1", "3", "3"],
    memory: ["4000", "1800", "1000"],
    result: "0,1,2,4,3,5,6,7",
  },
  {
    // The tab "1" contains 4 frames, but its uniqueCount is 1 because
    // all of those frames are backed by the process "1".  As a result,
    // TabUnloader puts the tab "1" first based on the last access time.
    tabs: ["1", "2", "3", "4", "5"],
    process: ["1,1,1,1", "2", "3", "3", "3"],
    memory: ["100", "100", "100"],
    result: "0,1,2,3,4",
  },
  {
    // The uniqueCount of the tab "1", "2", and "3" is 1, 2, and 3,
    // respectively.  As a result the first three tabs are sorted as 2,1,0.
    tabs: ["1", "2", "3", "4", "5", "6"],
    process: ["1,7,1,7,1,1,7,1", "7,3,7,2", "4,5,7,4,6,7", "7", "7", "7"],
    memory: ["100", "100", "100", "100", "100", "100", "100"],
    result: "2,1,0,3,4,5",
  },
];

let globalBrowser = {
  discardBrowser() {
    return true;
  },
};

add_task(async function doTests() {
  for (let test of unloadTests) {
    function* iterateTabs() {
      let tabs = test.tabs;
      for (let t = 0; t < tabs.length; t++) {
        let tab = {
          tab: {
            originalIndex: t,
            lastAccessed: Number(/^[0-9]+/.exec(tabs[t])[0]),
            keywords: tabs[t],
            process: "process" in test ? test.process[t] : "1",
          },
          memory: test.memory,
          gBrowser: globalBrowser,
        };
        yield tab;
      }
    }
    TestTabUnloaderMethods.iterateTabs = iterateTabs;

    let expectedOrder = "";
    const sortedTabs = await TabUnloader.getSortedTabs(
      0,
      TestTabUnloaderMethods
    );
    for (let tab of sortedTabs) {
      if (expectedOrder) {
        expectedOrder += ",";
      }
      expectedOrder += tab.tab.originalIndex;
    }

    Assert.equal(expectedOrder, test.result);
  }
});
