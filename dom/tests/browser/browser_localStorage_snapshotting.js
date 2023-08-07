const HELPER_PAGE_URL =
  "https://example.com/browser/dom/tests/browser/page_localstorage_snapshotting.html";
const HELPER_PAGE_ORIGIN = "https://example.com/";

/* import-globals-from helper_localStorage.js */

let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "/helper_localStorage.js", this);

function clearOrigin() {
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      HELPER_PAGE_ORIGIN
    );
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

async function applyMutations(knownTab, mutations) {
  await SpecialPowers.spawn(
    knownTab.tab.linkedBrowser,
    [mutations],
    function (mutations) {
      return content.wrappedJSObject.applyMutations(
        Cu.cloneInto(mutations, content)
      );
    }
  );
}

async function verifyState(knownTab, expectedState) {
  let actualState = await SpecialPowers.spawn(
    knownTab.tab.linkedBrowser,
    [],
    function () {
      return content.wrappedJSObject.getState();
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

async function getKeys(knownTab) {
  let keys = await SpecialPowers.spawn(
    knownTab.tab.linkedBrowser,
    [],
    function () {
      return content.wrappedJSObject.getKeys();
    }
  );
  return keys;
}

async function beginExplicitSnapshot(knownTab) {
  await SpecialPowers.spawn(knownTab.tab.linkedBrowser, [], function () {
    return content.wrappedJSObject.beginExplicitSnapshot();
  });
}

async function checkpointExplicitSnapshot(knownTab) {
  await SpecialPowers.spawn(knownTab.tab.linkedBrowser, [], function () {
    return content.wrappedJSObject.checkpointExplicitSnapshot();
  });
}

async function endExplicitSnapshot(knownTab) {
  await SpecialPowers.spawn(knownTab.tab.linkedBrowser, [], function () {
    return content.wrappedJSObject.endExplicitSnapshot();
  });
}

async function verifyHasSnapshot(knownTab, expectedHasSnapshot) {
  let hasSnapshot = await SpecialPowers.spawn(
    knownTab.tab.linkedBrowser,
    [],
    function () {
      return content.wrappedJSObject.getHasSnapshot();
    }
  );
  is(hasSnapshot, expectedHasSnapshot, "Correct has snapshot");
}

async function verifySnapshotUsage(knownTab, expectedSnapshotUsage) {
  let snapshotUsage = await SpecialPowers.spawn(
    knownTab.tab.linkedBrowser,
    [],
    function () {
      return content.wrappedJSObject.getSnapshotUsage();
    }
  );
  is(snapshotUsage, expectedSnapshotUsage, "Correct snapshot usage");
}

async function verifyParentState(expectedState) {
  let principal =
    Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      HELPER_PAGE_ORIGIN
    );

  let actualState = await Services.domStorageManager.getState(principal);

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

// We spin up a ton of child processes.
requestLongerTimeout(4);

/**
 * Verify snapshotting of our localStorage implementation in multi-e10s setup.
 */
add_task(async function () {
  if (!Services.domStorageManager.nextGenLocalStorageEnabled) {
    ok(true, "Test ignored when the next gen local storage is not enabled.");
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable LocalStorage's testing API so we can explicitly create
      // snapshots when needed.
      ["dom.storage.testing", true],
      // Force multiple web and webIsolated content processes so that the
      // multi-e10s logic works correctly.
      ["dom.ipc.processCount", 8],
      ["dom.ipc.processCount.webIsolated", 4],
    ],
  });

  // Ensure that there is no localstorage data by forcing the origin to be
  // cleared prior to the start of our test..
  await clearOrigin();

  // - Open tabs.  Don't configure any of them yet.
  const knownTabs = new KnownTabs();
  const writerTab1 = await openTestTab(
    HELPER_PAGE_URL,
    "writer1",
    knownTabs,
    true
  );
  const writerTab2 = await openTestTab(
    HELPER_PAGE_URL,
    "writer2",
    knownTabs,
    true
  );
  const readerTab1 = await openTestTab(
    HELPER_PAGE_URL,
    "reader1",
    knownTabs,
    true
  );
  const readerTab2 = await openTestTab(
    HELPER_PAGE_URL,
    "reader2",
    knownTabs,
    true
  );

  const initialMutations = [
    [null, null],
    ["key1", "initial1"],
    ["key2", "initial2"],
    ["key3", "initial3"],
    ["key5", "initial5"],
    ["key6", "initial6"],
    ["key7", "initial7"],
    ["key8", "initial8"],
  ];

  const initialState = {
    key1: "initial1",
    key2: "initial2",
    key3: "initial3",
    key5: "initial5",
    key6: "initial6",
    key7: "initial7",
    key8: "initial8",
  };

  let sizeOfOneKey;
  let sizeOfOneValue;
  let sizeOfOneItem;
  let sizeOfKeys = 0;
  let sizeOfItems = 0;

  let entries = Object.entries(initialState);
  for (let i = 0; i < entries.length; i++) {
    let entry = entries[i];
    let sizeOfKey = entry[0].length;
    let sizeOfValue = entry[1].length;
    let sizeOfItem = sizeOfKey + sizeOfValue;
    if (i == 0) {
      sizeOfOneKey = sizeOfKey;
      sizeOfOneValue = sizeOfValue;
      sizeOfOneItem = sizeOfItem;
    }
    sizeOfKeys += sizeOfKey;
    sizeOfItems += sizeOfItem;
  }

  info("Size of one key is " + sizeOfOneKey);
  info("Size of one value is " + sizeOfOneValue);
  info("Size of one item is " + sizeOfOneItem);
  info("Size of keys is " + sizeOfKeys);
  info("Size of items is " + sizeOfItems);

  const prefillValues = [
    // Zero prefill (prefill disabled)
    0,
    // Less than one key length prefill
    sizeOfOneKey - 1,
    // Greater than one key length and less than one item length prefill
    sizeOfOneKey + 1,
    // Precisely one item length prefill
    sizeOfOneItem,
    // Precisely two times one item length prefill
    2 * sizeOfOneItem,
    // Precisely size of keys prefill
    sizeOfKeys,
    // Less than size of keys plus one value length prefill
    sizeOfKeys + sizeOfOneValue - 1,
    // Precisely size of keys plus one value length prefill
    sizeOfKeys + sizeOfOneValue,
    // Greater than size of keys plus one value length and less than size of
    // keys plus two times one value length prefill
    sizeOfKeys + sizeOfOneValue + 1,
    // Precisely size of keys plus two times one value length prefill
    sizeOfKeys + 2 * sizeOfOneValue,
    // Precisely size of keys plus three times one value length prefill
    sizeOfKeys + 3 * sizeOfOneValue,
    // Precisely size of keys plus four times one value length prefill
    sizeOfKeys + 4 * sizeOfOneValue,
    // Precisely size of keys plus five times one value length prefill
    sizeOfKeys + 5 * sizeOfOneValue,
    // Precisely size of keys plus six times one value length prefill
    sizeOfKeys + 6 * sizeOfOneValue,
    // Precisely size of items prefill
    sizeOfItems,
    // Unlimited prefill
    -1,
  ];

  for (let prefillValue of prefillValues) {
    info("Setting prefill value to " + prefillValue);

    await SpecialPowers.pushPrefEnv({
      set: [["dom.storage.snapshot_prefill", prefillValue]],
    });

    const gradualPrefillValues = [
      // Zero gradual prefill
      0,
      // Less than one key length gradual prefill
      sizeOfOneKey - 1,
      // Greater than one key length and less than one item length gradual
      // prefill
      sizeOfOneKey + 1,
      // Precisely one item length gradual prefill
      sizeOfOneItem,
      // Precisely two times one item length gradual prefill
      2 * sizeOfOneItem,
      // Precisely three times one item length gradual prefill
      3 * sizeOfOneItem,
      // Precisely four times one item length gradual prefill
      4 * sizeOfOneItem,
      // Precisely five times one item length gradual prefill
      5 * sizeOfOneItem,
      // Precisely six times one item length gradual prefill
      6 * sizeOfOneItem,
      // Precisely size of items prefill
      sizeOfItems,
      // Unlimited gradual prefill
      -1,
    ];

    for (let gradualPrefillValue of gradualPrefillValues) {
      info("Setting gradual prefill value to " + gradualPrefillValue);

      await SpecialPowers.pushPrefEnv({
        set: [["dom.storage.snapshot_gradual_prefill", gradualPrefillValue]],
      });

      info("Stage 1");

      const setRemoveMutations1 = [
        ["key0", "setRemove10"],
        ["key1", "setRemove11"],
        ["key2", null],
        ["key3", "setRemove13"],
        ["key4", "setRemove14"],
        ["key5", "setRemove15"],
        ["key6", "setRemove16"],
        ["key7", "setRemove17"],
        ["key8", null],
        ["key9", "setRemove19"],
      ];

      const setRemoveState1 = {
        key0: "setRemove10",
        key1: "setRemove11",
        key3: "setRemove13",
        key4: "setRemove14",
        key5: "setRemove15",
        key6: "setRemove16",
        key7: "setRemove17",
        key9: "setRemove19",
      };

      const setRemoveMutations2 = [
        ["key0", "setRemove20"],
        ["key1", null],
        ["key2", "setRemove22"],
        ["key3", "setRemove23"],
        ["key4", "setRemove24"],
        ["key5", "setRemove25"],
        ["key6", "setRemove26"],
        ["key7", null],
        ["key8", "setRemove28"],
        ["key9", "setRemove29"],
      ];

      const setRemoveState2 = {
        key0: "setRemove20",
        key2: "setRemove22",
        key3: "setRemove23",
        key4: "setRemove24",
        key5: "setRemove25",
        key6: "setRemove26",
        key8: "setRemove28",
        key9: "setRemove29",
      };

      // Apply initial mutations using an explicit snapshot. The explicit
      // snapshot here ensures that the parent process have received the
      // changes.
      await beginExplicitSnapshot(writerTab1);
      await applyMutations(writerTab1, initialMutations);
      await endExplicitSnapshot(writerTab1);

      // Begin explicit snapshots in all tabs except readerTab2. All these tabs
      // should see the initial state regardless what other tabs are doing.
      await beginExplicitSnapshot(writerTab1);
      await beginExplicitSnapshot(writerTab2);
      await beginExplicitSnapshot(readerTab1);

      // Apply first array of set/remove mutations in writerTab1 and end the
      // explicit snapshot. This will trigger saving of values in other active
      // snapshots.
      await applyMutations(writerTab1, setRemoveMutations1);
      await endExplicitSnapshot(writerTab1);

      // Begin an explicit snapshot in readerTab2. writerTab1 already ended its
      // explicit snapshot, so readerTab2 should see mutations done by
      // writerTab1.
      await beginExplicitSnapshot(readerTab2);

      // Apply second array of set/remove mutations in writerTab2 and end the
      // explicit snapshot. This will trigger saving of values in other active
      // snapshots, but only if they haven't been saved already.
      await applyMutations(writerTab2, setRemoveMutations2);
      await endExplicitSnapshot(writerTab2);

      // Verify state in readerTab1, it should match the initial state.
      await verifyState(readerTab1, initialState);
      await endExplicitSnapshot(readerTab1);

      // Verify state in readerTab2, it should match the state after the first
      // array of set/remove mutatations have been applied and "commited".
      await verifyState(readerTab2, setRemoveState1);
      await endExplicitSnapshot(readerTab2);

      // Verify final state, it should match the state after the second array of
      // set/remove mutation have been applied and "commited". An explicit
      // snapshot is used.
      await beginExplicitSnapshot(readerTab1);
      await verifyState(readerTab1, setRemoveState2);
      await endExplicitSnapshot(readerTab1);

      info("Stage 2");

      const setRemoveClearMutations1 = [
        ["key0", "setRemoveClear10"],
        ["key1", null],
        [null, null],
      ];

      const setRemoveClearState1 = {};

      const setRemoveClearMutations2 = [
        ["key8", null],
        ["key9", "setRemoveClear29"],
        [null, null],
      ];

      const setRemoveClearState2 = {};

      // This is very similar to previous stage except that in addition to
      // set/remove, the clear operation is involved too.
      await beginExplicitSnapshot(writerTab1);
      await applyMutations(writerTab1, initialMutations);
      await endExplicitSnapshot(writerTab1);

      await beginExplicitSnapshot(writerTab1);
      await beginExplicitSnapshot(writerTab2);
      await beginExplicitSnapshot(readerTab1);

      await applyMutations(writerTab1, setRemoveClearMutations1);
      await endExplicitSnapshot(writerTab1);

      await beginExplicitSnapshot(readerTab2);

      await applyMutations(writerTab2, setRemoveClearMutations2);
      await endExplicitSnapshot(writerTab2);

      await verifyState(readerTab1, initialState);
      await endExplicitSnapshot(readerTab1);

      await verifyState(readerTab2, setRemoveClearState1);
      await endExplicitSnapshot(readerTab2);

      await beginExplicitSnapshot(readerTab1);
      await verifyState(readerTab1, setRemoveClearState2);
      await endExplicitSnapshot(readerTab1);

      info("Stage 3");

      const changeOrderMutations = [
        ["key1", null],
        ["key2", null],
        ["key3", null],
        ["key5", null],
        ["key6", null],
        ["key7", null],
        ["key8", null],
        ["key8", "initial8"],
        ["key7", "initial7"],
        ["key6", "initial6"],
        ["key5", "initial5"],
        ["key3", "initial3"],
        ["key2", "initial2"],
        ["key1", "initial1"],
      ];

      // Apply initial mutations using an explicit snapshot. The explicit
      // snapshot here ensures that the parent process have received the
      // changes.
      await beginExplicitSnapshot(writerTab1);
      await applyMutations(writerTab1, initialMutations);
      await endExplicitSnapshot(writerTab1);

      // Begin explicit snapshots in all tabs except writerTab2 which is not
      // used in this stage. All these tabs should see the initial order
      // regardless what other tabs are doing.
      await beginExplicitSnapshot(readerTab1);
      await beginExplicitSnapshot(writerTab1);
      await beginExplicitSnapshot(readerTab2);

      // Get all keys in readerTab1 and end the explicit snapshot. No mutations
      // have been applied yet.
      let tab1Keys = await getKeys(readerTab1);
      await endExplicitSnapshot(readerTab1);

      // Apply mutations that change the order of keys and end the explicit
      // snapshot. The state is unchanged. This will trigger saving of key order
      // in other active snapshots, but only if the order hasn't been saved
      // already.
      await applyMutations(writerTab1, changeOrderMutations);
      await endExplicitSnapshot(writerTab1);

      // Get all keys in readerTab2 and end the explicit snapshot. Change order
      // mutations have been applied, but the order should stay unchanged.
      let tab2Keys = await getKeys(readerTab2);
      await endExplicitSnapshot(readerTab2);

      // Verify the key order is the same.
      is(tab2Keys.length, tab1Keys.length, "Correct keys length");
      for (let i = 0; i < tab2Keys.length; i++) {
        is(tab2Keys[i], tab1Keys[i], "Correct key");
      }

      // Verify final state, it should match the initial state since applied
      // mutations only changed the key order. An explicit snapshot is used.
      await beginExplicitSnapshot(readerTab1);
      await verifyState(readerTab1, initialState);
      await endExplicitSnapshot(readerTab1);
    }
  }

  // - Clean up.
  await cleanupTabs(knownTabs);

  clearOrigin();
});

/**
 * Verify that snapshots are able to work with negative usage. This can happen
 * when there's an item stored in localStorage with given size and then two
 * snaphots (created at the same time) mutate the item. The first one replases
 * it with something bigger and the other one removes it.
 */
add_task(async function () {
  if (!Services.domStorageManager.nextGenLocalStorageEnabled) {
    ok(true, "Test ignored when the next gen local storage is not enabled.");
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      // Force multiple web and webIsolated content processes so that the
      // multi-e10s logic works correctly.
      ["dom.ipc.processCount", 4],
      ["dom.ipc.processCount.webIsolated", 2],
      // Disable snapshot peak usage pre-incrementation to make the testing
      // easier.
      ["dom.storage.snapshot_peak_usage.initial_preincrement", 0],
      ["dom.storage.snapshot_peak_usage.reduced_initial_preincrement", 0],
      ["dom.storage.snapshot_peak_usage.gradual_preincrement", 0],
      ["dom.storage.snapshot_peak_usage.reuced_gradual_preincrement", 0],
      // Enable LocalStorage's testing API so we can explicitly create
      // snapshots when needed.
      ["dom.storage.testing", true],
    ],
  });

  // Ensure that there is no localstorage data by forcing the origin to be
  // cleared prior to the start of our test.
  await clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);

  // Open tabs.  Don't configure any of them yet.
  const knownTabs = new KnownTabs();
  const writerTab1 = await openTestTab(
    HELPER_PAGE_URL,
    "writer1",
    knownTabs,
    true
  );
  const writerTab2 = await openTestTab(
    HELPER_PAGE_URL,
    "writer2",
    knownTabs,
    true
  );

  // Apply the initial mutation using an explicit snapshot. The explicit
  // snapshot here ensures that the parent process have received the changes.
  await beginExplicitSnapshot(writerTab1);
  await applyMutations(writerTab1, [["key", "something"]]);
  await endExplicitSnapshot(writerTab1);

  // Begin explicit snapshots in both tabs. Both tabs should see the initial
  // state.
  await beginExplicitSnapshot(writerTab1);
  await beginExplicitSnapshot(writerTab2);

  // Apply the first mutation in writerTab1 and end the explicit snapshot.
  await applyMutations(writerTab1, [["key", "somethingBigger"]]);
  await endExplicitSnapshot(writerTab1);

  // Apply the second mutation in writerTab2 and end the explicit snapshot.
  await applyMutations(writerTab2, [["key", null]]);
  await endExplicitSnapshot(writerTab2);

  // Verify the final state, it should match the state after the second
  // mutation has been applied and "commited". An explicit snapshot is used.
  await beginExplicitSnapshot(writerTab1);
  await verifyState(writerTab1, {});
  await endExplicitSnapshot(writerTab1);

  // Clean up.
  await cleanupTabs(knownTabs);

  clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);
});

/**
 * Verify that snapshot usage is correctly updated after each operation.
 */
add_task(async function () {
  if (!Services.domStorageManager.nextGenLocalStorageEnabled) {
    ok(true, "Test ignored when the next gen local storage is not enabled.");
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      // Force multiple web and webIsolated content processes so that the
      // multi-e10s logic works correctly.
      ["dom.ipc.processCount", 4],
      ["dom.ipc.processCount.webIsolated", 2],
      // Disable snapshot peak usage pre-incrementation to make the testing
      // easier.
      ["dom.storage.snapshot_peak_usage.initial_preincrement", 0],
      ["dom.storage.snapshot_peak_usage.reduced_initial_preincrement", 0],
      ["dom.storage.snapshot_peak_usage.gradual_preincrement", 0],
      ["dom.storage.snapshot_peak_usage.reuced_gradual_preincrement", 0],
      // Enable LocalStorage's testing API so we can explicitly create
      // snapshots when needed.
      ["dom.storage.testing", true],
    ],
  });

  // Ensure that there is no localstorage data by forcing the origin to be
  // cleared prior to the start of our test.
  await clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);

  // Open tabs.  Don't configure any of them yet.
  const knownTabs = new KnownTabs();
  const writerTab1 = await openTestTab(
    HELPER_PAGE_URL,
    "writer1",
    knownTabs,
    true
  );
  const writerTab2 = await openTestTab(
    HELPER_PAGE_URL,
    "writer2",
    knownTabs,
    true
  );

  // Apply the initial mutation using an explicit snapshot. The explicit
  // snapshot here ensures that the parent process have received the changes.
  await beginExplicitSnapshot(writerTab1);
  await verifySnapshotUsage(writerTab1, 0);
  await applyMutations(writerTab1, [["key", "something"]]);
  await verifySnapshotUsage(writerTab1, 12);
  await endExplicitSnapshot(writerTab1);
  await verifyHasSnapshot(writerTab1, false);

  // Begin an explicit snapshot in writerTab1 and apply the first mutatation
  // in it.
  await beginExplicitSnapshot(writerTab1);
  await verifySnapshotUsage(writerTab1, 12);
  await applyMutations(writerTab1, [["key", "somethingBigger"]]);
  await verifySnapshotUsage(writerTab1, 18);

  // Begin an explicit snapshot in writerTab2 and apply the second mutatation
  // in it.
  await beginExplicitSnapshot(writerTab2);
  await verifySnapshotUsage(writerTab2, 18);
  await applyMutations(writerTab2, [[null, null]]);
  await verifySnapshotUsage(writerTab2, 6);

  // End explicit snapshots in both tabs.
  await endExplicitSnapshot(writerTab1);
  await verifyHasSnapshot(writerTab1, false);
  await endExplicitSnapshot(writerTab2);
  await verifyHasSnapshot(writerTab2, false);

  // Verify the final state, it should match the state after the second
  // mutation has been applied and "commited". An explicit snapshot is used.
  await beginExplicitSnapshot(writerTab1);
  await verifySnapshotUsage(writerTab1, 0);
  await verifyState(writerTab1, {});
  await endExplicitSnapshot(writerTab1);
  await verifyHasSnapshot(writerTab1, false);

  // Clean up.
  await cleanupTabs(knownTabs);

  clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);
});

/**
 * Verify that datastore in the parent is correctly updated after a checkpoint.
 */
add_task(async function () {
  if (!Services.domStorageManager.nextGenLocalStorageEnabled) {
    ok(true, "Test ignored when the next gen local storage is not enabled.");
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      // Force multiple web and webIsolated content processes so that the
      // multi-e10s logic works correctly.
      ["dom.ipc.processCount", 4],
      ["dom.ipc.processCount.webIsolated", 2],
      // Enable LocalStorage's testing API so we can explicitly create
      // snapshots when needed.
      ["dom.storage.testing", true],
    ],
  });

  // Ensure that there is no localstorage data by forcing the origin to be
  // cleared prior to the start of our test.
  await clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);

  // Open tabs.  Don't configure any of them yet.
  const knownTabs = new KnownTabs();
  const writerTab1 = await openTestTab(
    HELPER_PAGE_URL,
    "writer1",
    knownTabs,
    true
  );

  await verifyParentState({});

  // Apply the initial mutation using an explicit snapshot. The explicit
  // snapshot here ensures that the parent process have received the changes.
  await beginExplicitSnapshot(writerTab1);
  await verifyParentState({});
  await applyMutations(writerTab1, [["key", "something"]]);
  await verifyParentState({});
  await endExplicitSnapshot(writerTab1);

  await verifyParentState({ key: "something" });

  // Begin an explicit snapshot in writerTab1, apply the first mutation in
  // writerTab1 and checkpoint the explicit snapshot.
  await beginExplicitSnapshot(writerTab1);
  await verifyParentState({ key: "something" });
  await applyMutations(writerTab1, [["key", "somethingBigger"]]);
  await verifyParentState({ key: "something" });
  await checkpointExplicitSnapshot(writerTab1);

  await verifyParentState({ key: "somethingBigger" });

  // Apply the second mutation in writerTab1 and checkpoint the explicit
  // snapshot.
  await applyMutations(writerTab1, [["key", null]]);
  await verifyParentState({ key: "somethingBigger" });
  await checkpointExplicitSnapshot(writerTab1);

  await verifyParentState({});

  // Apply the third mutation in writerTab1 and end the explicit snapshot.
  await applyMutations(writerTab1, [["otherKey", "something"]]);
  await verifyParentState({});
  await endExplicitSnapshot(writerTab1);

  await verifyParentState({ otherKey: "something" });

  // Verify the final state, it should match the state after the third mutation
  // has been applied and "commited". An explicit snapshot is used.
  await beginExplicitSnapshot(writerTab1);
  await verifyParentState({ otherKey: "something" });
  await verifyState(writerTab1, { otherKey: "something" });
  await endExplicitSnapshot(writerTab1);

  await verifyParentState({ otherKey: "something" });

  // Clean up.
  await cleanupTabs(knownTabs);

  clearOriginStorageEnsuringNoPreload(HELPER_PAGE_ORIGIN);
});
