/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function testSteps() {
  const data = {};
  data.key = "key1";
  data.value = "value1";
  data.usage = data.key.length + data.value.length;

  const principal = getPrincipal("http://example.com");

  info("Setting prefs");

  Services.prefs.setBoolPref(
    "dom.storage.enable_unsupported_legacy_implementation",
    false
  );

  info("Stage 1 - Testing usage after adding item");

  info("Getting storage");

  let storage = getLocalStorage(principal);

  info("Adding item");

  storage.setItem(data.key, data.value);

  info("Resetting origin");

  let request = resetOrigin(principal);
  await requestFinished(request);

  info("Getting usage");

  request = getOriginUsage(principal);
  await requestFinished(request);

  is(request.result.usage, data.usage, "Correct usage");

  info("Resetting");

  request = reset();
  await requestFinished(request);

  info("Stage 2 - Testing usage after removing item");

  info("Getting storage");

  storage = getLocalStorage(principal);

  info("Removing item");

  storage.removeItem(data.key);

  info("Resetting origin");

  request = resetOrigin(principal);
  await requestFinished(request);

  info("Getting usage");

  request = getOriginUsage(principal);
  await requestFinished(request);

  is(request.result.usage, 0, "Correct usage");
});
