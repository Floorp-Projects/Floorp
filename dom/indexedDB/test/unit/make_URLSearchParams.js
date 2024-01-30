/*
  Any copyright is dedicated to the Public Domain.
  http://creativecommons.org/publicdomain/zero/1.0/
*/

async function testSteps() {
  const name = "URLSearchParams";
  const options = { foo: "bar", baz: "bar" };
  const params = new URLSearchParams(options);
  const value = { urlSearchParams: params };
  const key = 42;

  info("Opening database");

  const request = indexedDB.open(name);
  await expectingUpgrade(request);

  const database = request.result;

  const objectStore = database.createObjectStore(name, {});

  info("Adding value");

  objectStore.add(value, key);

  await requestSucceeded(request);

  info("Closing database");

  database.close();
}
