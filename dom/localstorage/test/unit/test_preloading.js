/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function testSteps() {
  const principals = [
    getPrincipal("http://example.com", {}),
    getPrincipal("http://example.com", { privateBrowsingId: 1 }),
  ];

  async function isPreloaded(principal) {
    return Services.domStorageManager.isPreloaded(principal);
  }

  info("Setting prefs");

  Services.prefs.setBoolPref(
    "dom.storage.enable_unsupported_legacy_implementation",
    false
  );
  Services.prefs.setBoolPref("dom.storage.snapshot_reusing", false);

  for (const principal of principals) {
    info("Getting storage");

    let storage = getLocalStorage(principal);

    ok(
      !(await isPreloaded(principal)),
      "Data is not preloaded after getting storage"
    );

    info("Opening storage");

    storage.open();

    ok(await isPreloaded(principal), "Data is preloaded after opening storage");

    info("Closing storage");

    storage.close();

    if (principal.privateBrowsingId > 0) {
      ok(
        await isPreloaded(principal),
        "Data is still preloaded after closing storage"
      );

      info("Closing private session");

      Services.obs.notifyObservers(null, "last-pb-context-exited");

      ok(
        !(await isPreloaded(principal)),
        "Data is not preloaded anymore after closing private session"
      );
    } else {
      ok(
        !(await isPreloaded(principal)),
        "Data is not preloaded anymore after closing storage"
      );
    }

    info("Opening storage again");

    storage.open();

    ok(
      await isPreloaded(principal),
      "Data is preloaded after opening storage again"
    );

    info("Clearing origin");

    let request = clearOrigin(principal, "default");
    await requestFinished(request);

    ok(
      !(await isPreloaded(principal)),
      "Data is not preloaded after clearing origin"
    );
  }
});
