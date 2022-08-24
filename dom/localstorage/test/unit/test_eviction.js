/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function testSteps() {
  const globalLimitKB = 5 * 1024;

  const data = {};
  data.sizeKB = 1 * 1024;
  data.key = "A";
  data.value = repeatChar(data.sizeKB * 1024 - data.key.length, ".");
  data.urlCount = globalLimitKB / data.sizeKB;

  function getSpec(index) {
    return "http://example" + index + ".com";
  }

  info("Setting prefs");

  Services.prefs.setBoolPref(
    "dom.storage.enable_unsupported_legacy_implementation",
    false
  );
  Services.prefs.setBoolPref("dom.storage.snapshot_reusing", false);

  info("Setting limits");

  setGlobalLimit(globalLimitKB);

  let request = clear();
  await requestFinished(request);

  info("Getting storages");

  let storages = [];
  for (let i = 0; i < data.urlCount; i++) {
    let storage = getLocalStorage(getPrincipal(getSpec(i)));
    storages.push(storage);
  }

  info("Filling up entire default storage");

  for (let i = 0; i < data.urlCount; i++) {
    storages[i].setItem(data.key, data.value);
    await returnToEventLoop();
  }

  info("Verifying no more data can be written");

  for (let i = 0; i < data.urlCount; i++) {
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

  info("Closing first origin");

  storages[0].close();

  let principal = getPrincipal("http://example0.com");

  request = resetOrigin(principal);
  await requestFinished(request);

  info("Getting usage for first origin");

  request = getOriginUsage(principal);
  await requestFinished(request);

  is(request.result.usage, data.sizeKB * 1024, "Correct usage");

  info("Verifying more data data can be written");

  for (let i = 1; i < data.urlCount; i++) {
    storages[i].setItem("B", "");
  }

  info("Getting usage for first origin");

  request = getOriginUsage(principal);
  await requestFinished(request);

  is(request.result.usage, 0, "Zero usage");
});
