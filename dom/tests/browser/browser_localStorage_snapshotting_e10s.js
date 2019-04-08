const HELPER_PAGE_URL =
  "http://example.com/browser/dom/tests/browser/page_localstorage_snapshotting_e10s.html";
const HELPER_PAGE_ORIGIN = "http://example.com/";

/* import-globals-from helper_localStorage_e10s.js */

let testDir = gTestPath.substr(0, gTestPath.lastIndexOf("/"));
Services.scriptloader.loadSubScript(testDir + "/helper_localStorage_e10s.js",
                                    this);

function clearOrigin() {
  let principal =
    Services.scriptSecurityManager.createCodebasePrincipalFromOrigin(
      HELPER_PAGE_ORIGIN);
  let request =
    Services.qms.clearStoragesForPrincipal(principal, "default", "ls");
  let promise = new Promise(resolve => {
    request.callback = () => {
      resolve();
    };
  });
  return promise;
}

async function applyMutations(knownTab, mutations) {
  await ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    mutations,
    function(mutations) {
      return content.wrappedJSObject.applyMutations(Cu.cloneInto(mutations,
                                                                 content));
    });
}

async function verifyState(knownTab, expectedState) {
  let actualState = await ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    {},
    function() {
      return content.wrappedJSObject.getState();
    });

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
  let keys = await ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    null,
    function() {
      return content.wrappedJSObject.getKeys();
    });
  return keys;
}

async function beginExplicitSnapshot(knownTab) {
  await ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    null,
    function() {
      return content.wrappedJSObject.beginExplicitSnapshot();
    });
}

async function endExplicitSnapshot(knownTab) {
  await ContentTask.spawn(
    knownTab.tab.linkedBrowser,
    null,
    function() {
      return content.wrappedJSObject.endExplicitSnapshot();
    });
}

// We spin up a ton of child processes.
requestLongerTimeout(4);

/**
 * Verify snapshotting of our localStorage implementation in multi-e10s setup.
 */
add_task(async function() {
  if (!Services.lsm.nextGenLocalStorageEnabled) {
    ok(true, "Test ignored when the next gen local storage is not enabled.");
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      // Enable LocalStorage's testing API so we can explicitly create
      // snapshots when needed.
      ["dom.storage.testing", true],
    ],
  });

  // Ensure that there is no localstorage data by forcing the origin to be
  // cleared prior to the start of our test..
  await clearOrigin();

  // - Open tabs.  Don't configure any of them yet.
  const knownTabs = new KnownTabs();
  const writerTab1 = await openTestTabInOwnProcess(HELPER_PAGE_URL, "writer1",
    knownTabs);
  const writerTab2 = await openTestTabInOwnProcess(HELPER_PAGE_URL, "writer2",
    knownTabs);
  const readerTab1 = await openTestTabInOwnProcess(HELPER_PAGE_URL, "reader1",
    knownTabs);
  const readerTab2 = await openTestTabInOwnProcess(HELPER_PAGE_URL, "reader2",
    knownTabs);

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
      set: [
        ["dom.storage.snapshot_prefill", prefillValue],
      ],
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
        set: [
          ["dom.storage.snapshot_gradual_prefill", gradualPrefillValue],
        ],
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

      const setRemoveClearState1 = {
      };

      const setRemoveClearMutations2 = [
        ["key8", null],
        ["key9", "setRemoveClear29"],
        [null, null],
      ];

      const setRemoveClearState2 = {
      };

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
