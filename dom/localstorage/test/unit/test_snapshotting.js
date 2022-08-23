/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function testSteps() {
  const url = "http://example.com";

  info("Setting pref");

  Services.prefs.setBoolPref("dom.storage.snapshot_reusing", false);

  const items = [
    { key: "key01", value: "value01" },
    { key: "key02", value: "value02" },
    { key: "key03", value: "value03" },
    { key: "key04", value: "value04" },
    { key: "key05", value: "value05" },
    { key: "key06", value: "value06" },
    { key: "key07", value: "value07" },
    { key: "key08", value: "value08" },
    { key: "key09", value: "value09" },
    { key: "key10", value: "value10" },
  ];

  let sizeOfOneKey;
  let sizeOfOneValue;
  let sizeOfOneItem;
  let sizeOfKeys = 0;
  let sizeOfItems = 0;

  for (let i = 0; i < items.length; i++) {
    let item = items[i];
    let sizeOfKey = item.key.length;
    let sizeOfValue = item.value.length;
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
    // Precisely three times one item length prefill
    3 * sizeOfOneItem,
    // Precisely four times one item length prefill
    4 * sizeOfOneItem,
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
    // Precisely size of keys plus seven times one value length prefill
    sizeOfKeys + 7 * sizeOfOneValue,
    // Precisely size of keys plus eight times one value length prefill
    sizeOfKeys + 8 * sizeOfOneValue,
    // Precisely size of keys plus nine times one value length prefill
    sizeOfKeys + 9 * sizeOfOneValue,
    // Precisely size of items prefill
    sizeOfItems,
    // Unlimited prefill
    -1,
  ];

  for (let prefillValue of prefillValues) {
    info("Setting prefill value to " + prefillValue);

    Services.prefs.setIntPref("dom.storage.snapshot_prefill", prefillValue);

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
      // Precisely seven times one item length gradual prefill
      7 * sizeOfOneItem,
      // Precisely eight times one item length gradual prefill
      8 * sizeOfOneItem,
      // Precisely nine times one item length gradual prefill
      9 * sizeOfOneItem,
      // Precisely size of items prefill
      sizeOfItems,
      // Unlimited gradual prefill
      -1,
    ];

    for (let gradualPrefillValue of gradualPrefillValues) {
      info("Setting gradual prefill value to " + gradualPrefillValue);

      Services.prefs.setIntPref(
        "dom.storage.snapshot_gradual_prefill",
        gradualPrefillValue
      );

      info("Getting storage");

      let storage = getLocalStorage(getPrincipal(url));

      // 1st snapshot

      info("Adding data");

      for (let item of items) {
        storage.setItem(item.key, item.value);
      }

      info("Saving key order");

      // This forces GetKeys to be called internally.
      let savedKeys = Object.keys(storage);

      // GetKey should match GetKeys
      for (let i = 0; i < savedKeys.length; i++) {
        is(storage.key(i), savedKeys[i], "Correct key");
      }

      info("Returning to event loop");

      // Returning to event loop forces the internal snapshot to finish.
      await returnToEventLoop();

      // 2nd snapshot

      info("Verifying length");

      is(storage.length, items.length, "Correct length");

      info("Verifying key order");

      let keys = Object.keys(storage);

      is(keys.length, savedKeys.length);

      for (let i = 0; i < keys.length; i++) {
        is(keys[i], savedKeys[i], "Correct key");
      }

      info("Verifying values");

      for (let item of items) {
        is(storage.getItem(item.key), item.value, "Correct value");
      }

      info("Returning to event loop");

      await returnToEventLoop();

      // 3rd snapshot

      // Force key2 to load.
      storage.getItem("key02");

      // Fill out write infos a bit.
      storage.removeItem("key05");
      storage.setItem("key05", "value05");
      storage.removeItem("key05");
      storage.setItem("key11", "value11");
      storage.setItem("key05", "value05");

      items.push({ key: "key11", value: "value11" });

      info("Verifying length");

      is(storage.length, items.length, "Correct length");

      // This forces to get all keys from the parent and then apply write infos
      // on already cached values.
      savedKeys = Object.keys(storage);

      info("Verifying values");

      for (let item of items) {
        is(storage.getItem(item.key), item.value, "Correct value");
      }

      storage.removeItem("key11");

      items.pop();

      info("Returning to event loop");

      await returnToEventLoop();

      // 4th snapshot

      // Force loading of all items.
      info("Verifying length");

      is(storage.length, items.length, "Correct length");

      info("Verifying values");

      for (let item of items) {
        is(storage.getItem(item.key), item.value, "Correct value");
      }

      is(storage.getItem("key11"), null, "Correct value");

      info("Returning to event loop");

      await returnToEventLoop();

      // 5th snapshot

      // Force loading of all keys.
      info("Saving key order");

      savedKeys = Object.keys(storage);

      // Force loading of all items.
      info("Verifying length");

      is(storage.length, items.length, "Correct length");

      info("Verifying values");

      for (let item of items) {
        is(storage.getItem(item.key), item.value, "Correct value");
      }

      is(storage.getItem("key11"), null, "Correct value");

      info("Returning to event loop");

      await returnToEventLoop();

      // 6th snapshot
      info("Verifying unknown item");

      is(storage.getItem("key11"), null, "Correct value");

      info("Verifying unknown item again");

      is(storage.getItem("key11"), null, "Correct value");

      info("Returning to event loop");

      await returnToEventLoop();

      // 7th snapshot

      // Save actual key order.
      info("Saving key order");

      savedKeys = Object.keys(storage);

      await returnToEventLoop();

      // 8th snapshot

      // Force loading of all items, but in reverse order.
      info("Getting values");

      for (let i = items.length - 1; i >= 0; i--) {
        let item = items[i];
        storage.getItem(item.key);
      }

      info("Verifying key order");

      keys = Object.keys(storage);

      is(keys.length, savedKeys.length);

      for (let i = 0; i < keys.length; i++) {
        is(keys[i], savedKeys[i], "Correct key");
      }

      await returnToEventLoop();

      // 9th snapshot

      info("Clearing");

      storage.clear();

      info("Returning to event loop");

      await returnToEventLoop();
    }
  }
});
