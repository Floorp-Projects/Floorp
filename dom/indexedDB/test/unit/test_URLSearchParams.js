/*
  Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/
*/

/* exported testSteps */
async function testSteps() {
  const name = "URLSearchParams";
  const options = { foo: "bar", baz: "bar" };
  const params = new URLSearchParams(options);
  const value = { urlSearchParams: params };
  const key = 42;

  info("Clearing");

  let request = clearAllDatabases();
  await requestFinished(request);

  info("Installing package");

  // The profile contains an IndexedDB database containing URLSearchParams.
  // The file make_URLSearchParams.js was run locally, specifically it was
  // temporarily enabled in xpcshell-parent-process.toml  and then executed:
  // mach test --interactive dom/indexedDB/test/unit/make_URLSearchParams.js
  installPackagedProfile("URLSearchParams_profile");

  info("Opening database");

  request = indexedDB.open(name);
  await requestSucceeded(request);

  const database = request.result;

  info("Getting value");

  request = database.transaction([name]).objectStore(name).get(key);
  await requestSucceeded(request);

  info("Verifying value");

  Assert.deepEqual(request.result, value, "Value is correctly structured");

  ok(
    request.result.urlSearchParams instanceof URLSearchParams,
    "Value urlSearchParams property is an instance of URLSearchParams"
  );

  info("Closing database");

  database.close();
}
