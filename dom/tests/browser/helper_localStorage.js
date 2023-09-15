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
    for (let key of this.byPid.keys()) {
      this.byPid[key] = null;
    }
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
async function openTestTab(
  helperPageUrl,
  name,
  knownTabs,
  shouldLoadInNewProcess
) {
  let realUrl = helperPageUrl + "?" + encodeURIComponent(name);
  // Load and wait for about:blank.
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:blank",
    forceNewProcess: true,
  });
  ok(!knownTabs.byName.has(name), "tab needs its own name: " + name);

  let knownTab = new KnownTab(name, tab);
  knownTabs.byName.set(name, knownTab);

  // Now trigger the actual load of our page.
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, realUrl);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let pid = tab.linkedBrowser.frameLoader.remoteTab.osPid;
  if (shouldLoadInNewProcess) {
    ok(
      !knownTabs.byPid.has(pid),
      "tab should be loaded in new process, pid: " + pid
    );
  } else {
    ok(
      knownTabs.byPid.has(pid),
      "tab should be loaded in the same process, new pid: " + pid
    );
  }

  if (knownTabs.byPid.has(pid)) {
    knownTabs.byPid.get(pid).set(name, knownTab);
  } else {
    let pidMap = new Map();
    pidMap.set(name, knownTab);
    knownTabs.byPid.set(pid, pidMap);
  }

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

/**
 * Wait for a LocalStorage flush to occur.  This notification can occur as a
 * result of any of:
 * - The normal, hardcoded 5-second flush timer.
 * - InsertDBOp seeing a preload op for an origin with outstanding changes.
 * - Us generating a "domstorage-test-flush-force" observer notification.
 */
function waitForLocalStorageFlush() {
  if (Services.domStorageManager.nextGenLocalStorageEnabled) {
    return new Promise(resolve => executeSoon(resolve));
  }

  return new Promise(function (resolve) {
    let observer = {
      observe() {
        SpecialPowers.removeObserver(observer, "domstorage-test-flushed");
        resolve();
      },
    };
    SpecialPowers.addObserver(observer, "domstorage-test-flushed");
  });
}

/**
 * Trigger and wait for a flush.  This is only necessary for forcing
 * mOriginsHavingData to be updated.  Normal operations exposed to content know
 * to automatically flush when necessary for correctness.
 *
 * The notification we're waiting for to verify flushing is fundamentally
 * ambiguous (see waitForLocalStorageFlush), so we actually trigger the flush
 * twice and wait twice.  In the event there was a race, there will be 3 flush
 * notifications, but correctness is guaranteed after the second notification.
 */
function triggerAndWaitForLocalStorageFlush() {
  if (Services.domStorageManager.nextGenLocalStorageEnabled) {
    return new Promise(resolve => executeSoon(resolve));
  }

  SpecialPowers.notifyObservers(null, "domstorage-test-flush-force");
  // This first wait is ambiguous...
  return waitForLocalStorageFlush().then(function () {
    // So issue a second flush and wait for that.
    SpecialPowers.notifyObservers(null, "domstorage-test-flush-force");
    return waitForLocalStorageFlush();
  });
}

/**
 * Clear the origin's storage so that "OriginsHavingData" will return false for
 * our origin.  Note that this is only the case for AsyncClear() which is
 * explicitly issued against a cache, or AsyncClearAll() which we can trigger
 * by wiping all storage.  However, the more targeted domain clearings that
 * we can trigger via observer, AsyncClearMatchingOrigin and
 * AsyncClearMatchingOriginAttributes will not clear the hashtable entry for
 * the origin.
 *
 * So we explicitly access the cache here in the parent for the origin and issue
 * an explicit clear.  Clearing all storage might be a little easier but seems
 * like asking for intermittent failures.
 */
function clearOriginStorageEnsuringNoPreload(origin) {
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(origin);

  if (Services.domStorageManager.nextGenLocalStorageEnabled) {
    let request = Services.qms.clearStoragesForPrincipal(
      principal,
      "default",
      "ls"
    );
    let promise = new Promise(resolve => {
      request.callback = () => {
        resolve();
      };
    });
    return promise;
  }

  // We want to use createStorage to force the cache to be created so we can
  // issue the clear.  It's possible for getStorage to return false but for the
  // origin preload hash to still have our origin in it.
  let storage = Services.domStorageManager.createStorage(
    null,
    principal,
    principal,
    ""
  );
  storage.clear();

  // We also need to trigger a flush os that mOriginsHavingData gets updated.
  // The inherent flush race is fine here because
  return triggerAndWaitForLocalStorageFlush();
}

async function verifyTabPreload(knownTab, expectStorageExists, origin) {
  let storageExists = await SpecialPowers.spawn(
    knownTab.tab.linkedBrowser,
    [origin],
    function (origin) {
      let principal =
        Services.scriptSecurityManager.createContentPrincipalFromOrigin(origin);
      if (Services.domStorageManager.nextGenLocalStorageEnabled) {
        return Services.domStorageManager.isPreloaded(principal);
      }
      return !!Services.domStorageManager.getStorage(
        null,
        principal,
        principal
      );
    }
  );
  is(storageExists, expectStorageExists, "Storage existence === preload");
}

/**
 * Instruct the given tab to execute the given series of mutations.  For
 * simplicity, the mutations representation matches the expected events rep.
 */
async function mutateTabStorage(knownTab, mutations, sentinelValue) {
  await SpecialPowers.spawn(
    knownTab.tab.linkedBrowser,
    [{ mutations, sentinelValue }],
    function (args) {
      return content.wrappedJSObject.mutateStorage(Cu.cloneInto(args, content));
    }
  );
}

/**
 * Instruct the given tab to add a "storage" event listener and record all
 * received events.  verifyTabStorageEvents is the corresponding method to
 * check and assert the recorded events.
 */
async function recordTabStorageEvents(knownTab, sentinelValue) {
  await SpecialPowers.spawn(
    knownTab.tab.linkedBrowser,
    [sentinelValue],
    function (sentinelValue) {
      return content.wrappedJSObject.listenForStorageEvents(sentinelValue);
    }
  );
}

/**
 * Retrieve the current localStorage contents perceived by the tab and assert
 * that they match the provided expected state.
 *
 * If maybeSentinel is non-null, it's assumed to be a string that identifies the
 * value we should be waiting for the sentinel key to take on.  This is
 * necessary because we cannot make any assumptions about when state will be
 * propagated to the given process.  See the comments in
 * page_localstorage_e10s.js for more context.  In general, a sentinel value is
 * required for correctness unless the process in question is the one where the
 * writes were performed or verifyTabStorageEvents was used.
 */
async function verifyTabStorageState(knownTab, expectedState, maybeSentinel) {
  let actualState = await SpecialPowers.spawn(
    knownTab.tab.linkedBrowser,
    [maybeSentinel],
    function (maybeSentinel) {
      return content.wrappedJSObject.getStorageState(maybeSentinel);
    }
  );

  for (let [expectedKey, expectedValue] of Object.entries(expectedState)) {
    ok(actualState.hasOwnProperty(expectedKey), "key present: " + expectedKey);
    is(actualState[expectedKey], expectedValue, "value correct");
  }
  for (let actualKey of Object.keys(actualState)) {
    if (!expectedState.hasOwnProperty(actualKey)) {
      ok(false, "actual state has key it shouldn't have: " + actualKey);
    }
  }
}

/**
 * Retrieve and clear the storage events recorded by the tab and assert that
 * they match the provided expected events.  For simplicity, the expected events
 * representation is the same as that used by mutateTabStorage.
 *
 * Note that by convention for test readability we are passed a 3rd argument of
 * the sentinel value, but we don't actually care what it is.
 */
async function verifyTabStorageEvents(knownTab, expectedEvents) {
  let actualEvents = await SpecialPowers.spawn(
    knownTab.tab.linkedBrowser,
    [],
    function () {
      return content.wrappedJSObject.returnAndClearStorageEvents();
    }
  );

  is(actualEvents.length, expectedEvents.length, "right number of events");
  for (let i = 0; i < actualEvents.length; i++) {
    let [actualKey, actualNewValue, actualOldValue] = actualEvents[i];
    let [expectedKey, expectedNewValue, expectedOldValue] = expectedEvents[i];
    is(actualKey, expectedKey, "keys match");
    is(actualNewValue, expectedNewValue, "new values match");
    is(actualOldValue, expectedOldValue, "old values match");
  }
}
