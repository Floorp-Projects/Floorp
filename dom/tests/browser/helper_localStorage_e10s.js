/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Simple tab wrapper abstracting our messaging mechanism;
class KnownTab {
  constructor(name, tab) {
    this.name = name;
    this.tab = tab;
  }

  cleanup() {
    this.tab = null;
  }
}

// Simple data structure class to help us track opened tabs and their pids.
class KnownTabs {
  constructor() {
    this.byPid = new Map();
    this.byName = new Map();
  }

  cleanup() {
    this.byPid = null;
    this.byName = null;
  }
}

/**
 * Open our helper page in a tab in its own content process, asserting that it
 * really is in its own process.  We initially load and wait for about:blank to
 * load, and only then loadURI to our actual page.  This is to ensure that
 * LocalStorageManager has had an opportunity to be created and populate
 * mOriginsHavingData.
 *
 * (nsGlobalWindow will reliably create LocalStorageManager as a side-effect of
 * the unconditional call to nsGlobalWindow::PreloadLocalStorage.  This will
 * reliably create the StorageDBChild instance, and its corresponding
 * StorageDBParent will send the set of origins when it is constructed.)
 */
async function openTestTabInOwnProcess(helperPageUrl, name, knownTabs) {
  let realUrl = helperPageUrl + "?" + encodeURIComponent(name);
  // Load and wait for about:blank.
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser, opening: "about:blank", forceNewProcess: true,
  });
  let pid = tab.linkedBrowser.frameLoader.remoteTab.osPid;
  ok(!knownTabs.byName.has(name), "tab needs its own name: " + name);
  ok(!knownTabs.byPid.has(pid), "tab needs to be in its own process: " + pid);

  let knownTab = new KnownTab(name, tab);
  knownTabs.byPid.set(pid, knownTab);
  knownTabs.byName.set(name, knownTab);

  // Now trigger the actual load of our page.
  BrowserTestUtils.loadURI(tab.linkedBrowser, realUrl);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  is(tab.linkedBrowser.frameLoader.remoteTab.osPid, pid, "still same pid");
  return knownTab;
}

/**
 * Close all the tabs we opened.
 */
async function cleanupTabs(knownTabs) {
  for (let knownTab of knownTabs.byName.values()) {
    BrowserTestUtils.removeTab(knownTab.tab);
    knownTab.cleanup();
  }
  knownTabs.cleanup();
}
