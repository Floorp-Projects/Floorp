/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps()
{
  const url = "http://example.com";

  const items = [
    { key: "key1", value: "value1" },
    { key: "key2", value: "value2" },
    { key: "key3", value: "value3" },
    { key: "key4", value: "value4" },
    { key: "key5", value: "value5" },
    { key: "key6", value: "value6" },
    { key: "key7", value: "value7" },
    { key: "key8", value: "value8" },
    { key: "key9", value: "value9" },
    { key: "key10", value: "value10" }
  ];

  function getPartialPrefill()
  {
    let size = 0;
    for (let i = 0; i < items.length / 2; i++) {
      let item = items[i];
      size += item.key.length + item.value.length;
    }
    return size;
  }

  const prefillValues = [
    0,                   // no prefill
    getPartialPrefill(), // partial prefill
    -1,                  // full prefill
  ];

  info("Setting pref");

  Services.prefs.setBoolPref("dom.storage.snapshot_reusing", false);

  for (let prefillValue of prefillValues) {
    info("Setting prefill value");

    Services.prefs.setIntPref("dom.storage.snapshot_prefill", prefillValue);

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
    storage.getItem("key2");

    // Fill out write infos a bit.
    storage.removeItem("key5");
    storage.setItem("key5", "value5");
    storage.removeItem("key5");
    storage.setItem("key11", "value11");
    storage.setItem("key5", "value5");

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
