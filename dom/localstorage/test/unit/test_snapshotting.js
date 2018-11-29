/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var testGenerator = testSteps();

function* testSteps()
{
  const url = "http://example.com";

  const items = [
    { key: "key1", value: "value1" },
    { key: "key2", value: "value2" },
    { key: "key3", value: "value3" },
    { key: "key4", value: "value4" },
    { key: "key5", value: "value5" }
  ];

  info("Getting storage");

  let storage = getLocalStorage(getPrincipal(url));

  info("Adding data");

  for (let item of items) {
    storage.setItem(item.key, item.value);
  }

  info("Saving key order");

  let keys = [];
  for (let i = 0; i < items.length; i++) {
    keys.push(storage.key(i));
  }

  // Let the internal snapshot finish by returning to the event loop.
  continueToNextStep();
  yield undefined;

  is(storage.length, items.length, "Correct length");

  info("Verifying key order");

  for (let i = 0; i < items.length; i++) {
    is(storage.key(i), keys[i], "Correct key");
  }

  finishTest();
}
