/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const url = "http://example.com";

  info("Setting pref");

  Services.prefs.setBoolPref("dom.storage.snapshot_reusing", false);

  const items = [
    { key: "key01", value: "value01" },
    { key: "key02", value: "value02" },
    { key: "key03", value: "value03" },
    { key: "key04", value: "value04" },
    { key: "key05", value: "value05" },
  ];

  info("Getting storage");

  let storage = getLocalStorage(getPrincipal(url));

  // 1st snapshot

  info("Adding data");

  for (let item of items) {
    storage.setItem(item.key, item.value);
  }

  info("Returning to event loop");

  await returnToEventLoop();

  // 2nd snapshot

  // Remove first two items, add some new items and add the two items back.

  storage.removeItem("key01");
  storage.removeItem("key02");

  storage.setItem("key06", "value06");
  storage.setItem("key07", "value07");
  storage.setItem("key08", "value08");

  storage.setItem("key01", "value01");
  storage.setItem("key02", "value02");

  info("Saving key order");

  let savedKeys = Object.keys(storage);

  info("Returning to event loop");

  await returnToEventLoop();

  // 3rd snapshot

  info("Verifying key order");

  let keys = Object.keys(storage);

  is(keys.length, savedKeys.length);

  for (let i = 0; i < keys.length; i++) {
    is(keys[i], savedKeys[i], "Correct key");
  }
}
