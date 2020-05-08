/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

async function testSteps() {
  const principal = getPrincipal("https://example.com");

  info("Setting prefs");

  SpecialPowers.setBoolPref("dom.storage.next_gen", true);
  Services.prefs.setBoolPref("dom.storage.client_validation", false);

  info("Clearing");

  let request = clear();
  await requestFinished(request);

  info("Creating simpledb");

  let database = getSimpleDatabase(principal);

  request = database.open("data");
  await requestFinished(request);

  info("Creating localStorage");

  let storage = Services.domStorageManager.createStorage(
    null,
    principal,
    principal,
    ""
  );
  storage.setItem("key", "value");

  info("Clearing simpledb");

  request = clearClient(principal, "default", "sdb");
  await requestFinished(request);

  info("Resetting localStorage");

  request = resetClient(principal, "ls");
  await requestFinished(request);
}
