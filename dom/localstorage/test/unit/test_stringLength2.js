/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that string length is correctly computed for
 * database values containing NULs. See bug 1541681.
 */

async function testSteps() {
  const principal = getPrincipal("http://example.org");

  const data = {};
  data.key = "foobar";
  data.secondKey = "foobaz";
  data.value = {
    length: 19253,
  };
  data.usage = data.key.length + data.value.length;

  async function checkUsage(expectedUsage) {
    info("Checking usage");

    // This forces any pending changes to be flushed to disk.  It also forces
    // data to be reloaded from disk at next localStorage API call.
    request = resetOrigin(principal);
    await requestFinished(request);

    request = getOriginUsage(principal);
    await requestFinished(request);

    is(request.result.usage, expectedUsage, "Correct usage");
  }

  info("Setting pref");

  Services.prefs.setBoolPref("dom.storage.next_gen", true);

  info("Stage 1 - Checking usage after profile installation");

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Installing package");

  // The profile contains storage.sqlite and webappsstore.sqlite.
  installPackage("stringLength2_profile");

  await checkUsage(0);

  info("Stage 2 - Checking usage after archived data migration");

  info("Opening database");

  let storage = getLocalStorage(principal);
  storage.open();

  await checkUsage(data.usage);

  info("Stage 3 - Checking usage after copying the value");

  info("Adding a second copy of the value");

  let value = storage.getItem(data.key);
  storage.setItem(data.secondKey, value);

  await checkUsage(2 * data.usage);

  info("Stage 4 - Checking length of the copied value");

  value = storage.getItem(data.secondKey);
  ok(value.length === data.value.length, "Correct string length");
}
