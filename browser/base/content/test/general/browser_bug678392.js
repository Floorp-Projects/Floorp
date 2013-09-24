/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let HTTPROOT = "http://example.com/browser/browser/base/content/test/general/";

function maxSnapshotOverride() {
  return 5;
}

function test() {
  waitForExplicitFinish();

  BrowserOpenTab();
  let tab = gBrowser.selectedTab;
  registerCleanupFunction(function () { gBrowser.removeTab(tab); });

  ok(gHistorySwipeAnimation, "gHistorySwipeAnimation exists.");

  if (!gHistorySwipeAnimation._isSupported()) {
    is(gHistorySwipeAnimation.active, false, "History swipe animation is not " +
       "active when not supported by the platform.");
    finish();
    return;
  }

  gHistorySwipeAnimation._getMaxSnapshots = maxSnapshotOverride;
  gHistorySwipeAnimation.init();

  is(gHistorySwipeAnimation.active, true, "History swipe animation support " +
     "was successfully initialized when supported.");

  cleanupArray();
  load(gBrowser.selectedTab, HTTPROOT + "browser_bug678392-2.html", test0);
}

function load(aTab, aUrl, aCallback) {
  aTab.linkedBrowser.addEventListener("load", function onload(aEvent) {
    aEvent.currentTarget.removeEventListener("load", onload, true);
    waitForFocus(aCallback, content);
  }, true);
  aTab.linkedBrowser.loadURI(aUrl);
}

function cleanupArray() {
  let arr = gHistorySwipeAnimation._trackedSnapshots;
  while (arr.length > 0) {
    delete arr[0].browser.snapshots[arr[0].index]; // delete actual snapshot
    arr.splice(0, 1);
  }
}

function testArrayCleanup() {
  // Test cleanup of array of tracked snapshots.
  let arr = gHistorySwipeAnimation._trackedSnapshots;
  is(arr.length, 0, "Snapshots were removed correctly from the array of " +
                    "tracked snapshots.");
}

function test0() {
  // Test growing of array of tracked snapshots.
  let tab = gBrowser.selectedTab;

  load(tab, HTTPROOT + "browser_bug678392-1.html", function() {
    ok(gHistorySwipeAnimation._trackedSnapshots, "Array for snapshot " +
      "tracking is initialized.");
    is(gHistorySwipeAnimation._trackedSnapshots.length, 1, "Snapshot array " +
       "has correct length of 1 after loading one page.");
    load(tab, HTTPROOT + "browser_bug678392-2.html", function() {
      is(gHistorySwipeAnimation._trackedSnapshots.length, 2, "Snapshot array " +
         " has correct length of 2 after loading two pages.");
      load(tab, HTTPROOT + "browser_bug678392-1.html", function() {
        is(gHistorySwipeAnimation._trackedSnapshots.length, 3, "Snapshot " +
           "array has correct length of 3 after loading three pages.");
        load(tab, HTTPROOT + "browser_bug678392-2.html", function() {
          is(gHistorySwipeAnimation._trackedSnapshots.length, 4, "Snapshot " +
             "array has correct length of 4 after loading four pages.");
          cleanupArray();
          testArrayCleanup();
          test1();
        });
      });
    });
  });
}

function verifyRefRemoved(aIndex, aBrowser) {
  let wasFound = false;
  let arr = gHistorySwipeAnimation._trackedSnapshots;
  for (let i = 0; i < arr.length; i++) {
    if (arr[i].index == aIndex && arr[i].browser == aBrowser)
      wasFound = true;
  }
  is(wasFound, false, "The reference that was previously removed was " +
     "still found in the array of tracked snapshots.");
}

function test1() {
  // Test presence of snpashots in per-tab array of snapshots and removal of
  // individual snapshots (and corresponding references in the array of
  // tracked snapshots).
  let tab = gBrowser.selectedTab;

  load(tab, HTTPROOT + "browser_bug678392-1.html", function() {
    var historyIndex = gBrowser.webNavigation.sessionHistory.index - 1;
    load(tab, HTTPROOT + "browser_bug678392-2.html", function() {
      load(tab, HTTPROOT + "browser_bug678392-1.html", function() {
        load(tab, HTTPROOT + "browser_bug678392-2.html", function() {
          let browser = gBrowser.selectedBrowser;
          ok(browser.snapshots, "Array of snapshots exists in browser.");
          ok(browser.snapshots[historyIndex], "First page exists in snapshot " +
                                              "array.");
          ok(browser.snapshots[historyIndex + 1], "Second page exists in " +
                                                  "snapshot array.");
          ok(browser.snapshots[historyIndex + 2], "Third page exists in " +
                                                  "snapshot array.");
          ok(browser.snapshots[historyIndex + 3], "Fourth page exists in " +
                                                  "snapshot array.");
          is(gHistorySwipeAnimation._trackedSnapshots.length, 4, "Length of " +
             "array of tracked snapshots is equal to 4 after loading four " +
             "pages.");

          // Test removal of reference in the middle of the array.
          gHistorySwipeAnimation._removeTrackedSnapshot(historyIndex + 1,
                                                        browser);
          verifyRefRemoved(historyIndex + 1, browser);
          is(gHistorySwipeAnimation._trackedSnapshots.length, 3, "Length of " +
             "array of tracked snapshots is equal to 3 after removing one" +
             "reference from the array with length 4.");

          // Test removal of reference at end of array.
          gHistorySwipeAnimation._removeTrackedSnapshot(historyIndex + 3,
                                                        browser);
          verifyRefRemoved(historyIndex + 3, browser);
          is(gHistorySwipeAnimation._trackedSnapshots.length, 2, "Length of " +
             "array of tracked snapshots is equal to 2 after removing two" +
             "references from the array with length 4.");

          // Test removal of reference at head of array.
          gHistorySwipeAnimation._removeTrackedSnapshot(historyIndex,
                                                        browser);
          verifyRefRemoved(historyIndex, browser);
          is(gHistorySwipeAnimation._trackedSnapshots.length, 1, "Length of " +
             "array of tracked snapshots is equal to 1 after removing three" +
             "references from the array with length 4.");

          cleanupArray();
          test2();
        });
      });
    });
  });
}

function test2() {
  // Test growing of snapshot array across tabs.
  let tab = gBrowser.selectedTab;

  load(tab, HTTPROOT + "browser_bug678392-1.html", function() {
    var historyIndex = gBrowser.webNavigation.sessionHistory.index - 1;
    load(tab, HTTPROOT + "browser_bug678392-2.html", function() {
      is(gHistorySwipeAnimation._trackedSnapshots.length, 2, "Length of " +
         "snapshot array is equal to 2 after loading two pages");
      let prevTab = tab;
      tab = gBrowser.addTab("about:newtab");
      gBrowser.selectedTab = tab;
      load(tab, HTTPROOT + "browser_bug678392-2.html" /* initial page */,
           function() {
        load(tab, HTTPROOT + "browser_bug678392-1.html", function() {
          load(tab, HTTPROOT + "browser_bug678392-2.html", function() {
            is(gHistorySwipeAnimation._trackedSnapshots.length, 4, "Length " +
               "of snapshot array is equal to 4 after loading two pages in " +
               "two tabs each.");
            gBrowser.removeCurrentTab();
            gBrowser.selectedTab = prevTab;
            cleanupArray();
            test3();
          });
        });
      });
    });
  });
}

function test3() {
  // Test uninit of gHistorySwipeAnimation.
  // This test MUST be the last one to execute.
  gHistorySwipeAnimation.uninit();
  is(gHistorySwipeAnimation.active, false, "History swipe animation support " +
     "was successfully uninitialized");
  finish();
}
