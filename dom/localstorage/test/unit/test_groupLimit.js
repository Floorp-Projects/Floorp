/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function testSteps() {
  const groupLimitKB = 10 * 1024;

  const globalLimitKB = groupLimitKB * 5;

  const originLimit = 10 * 1024;

  const urls = [
    "http://example.com",
    "http://test1.example.com",
    "https://test2.example.com",
    "http://test3.example.com:8080",
  ];

  const data = {};
  data.sizeKB = 5 * 1024;
  data.key = "A";
  data.value = repeatChar(data.sizeKB * 1024 - data.key.length, ".");
  data.urlCount = groupLimitKB / data.sizeKB;

  info("Setting pref");

  Services.prefs.setBoolPref(
    "dom.storage.enable_unsupported_legacy_implementation",
    false
  );
  Services.prefs.setBoolPref("dom.storage.snapshot_reusing", false);

  info("Setting limits");

  setGlobalLimit(globalLimitKB);

  let request = clear();
  await requestFinished(request);

  setOriginLimit(originLimit);

  info("Getting storages");

  let storages = [];
  for (let i = 0; i < urls.length; i++) {
    let storage = getLocalStorage(getPrincipal(urls[i]));
    storages.push(storage);
  }

  info("Filling up the whole group");

  for (let i = 0; i < data.urlCount; i++) {
    storages[i].setItem(data.key, data.value);
    await returnToEventLoop();
  }

  info("Verifying no more data can be written");

  for (let i = 0; i < urls.length; i++) {
    try {
      storages[i].setItem("B", "");
      ok(false, "Should have thrown");
    } catch (ex) {
      ok(true, "Did throw");
      ok(DOMException.isInstance(ex), "Threw DOMException");
      is(ex.name, "QuotaExceededError", "Threw right DOMException");
      is(ex.code, NS_ERROR_DOM_QUOTA_EXCEEDED_ERR, "Threw with right code");
    }
  }

  info("Clearing first origin");

  storages[0].clear();

  // Let the internal snapshot finish (usage is not descreased until all
  // snapshots finish)..
  await returnToEventLoop();

  info("Verifying more data can be written");

  for (let i = 0; i < urls.length; i++) {
    storages[i].setItem("B", "");
  }
});
