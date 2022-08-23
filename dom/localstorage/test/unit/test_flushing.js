/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

/**
 * This test is mainly to verify that the flush operation detaches the shadow
 * database in the event of early return due to error. See bug 1559029.
 */

add_task(async function testSteps() {
  const principal1 = getPrincipal("http://example1.com");

  const usageFile1 = getRelativeFile(
    "storage/default/http+++example1.com/ls/usage"
  );

  const principal2 = getPrincipal("http://example2.com");

  const data = {
    key: "foo",
    value: "bar",
  };

  const flushSleepTimeSec = 6;

  info("Setting prefs");

  Services.prefs.setBoolPref("dom.storage.next_gen", true);

  info("Getting storage 1");

  let storage1 = getLocalStorage(principal1);

  info("Adding item");

  storage1.setItem(data.key, data.value);

  info("Creating usage as a directory");

  // This will cause a failure during the flush for first principal.
  usageFile1.create(Ci.nsIFile.DIRECTORY_TYPE, parseInt("0755", 8));

  info("Getting storage 2");

  let storage2 = getLocalStorage(principal2);

  info("Adding item");

  storage2.setItem(data.key, data.value);

  // The flush for second principal shouldn't be affected by failed flush for
  // first principal.

  info(
    "Sleeping for " +
      flushSleepTimeSec +
      " seconds to let all flushes " +
      "finish"
  );

  await new Promise(function(resolve) {
    setTimeout(resolve, flushSleepTimeSec * 1000);
  });

  info("Resetting");

  // Wait for all database connections to close.
  let request = reset();
  await requestFinished(request);
});
