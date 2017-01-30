"use strict";

const {utils: Cu} = Components;
Cu.import("resource://shield-recipe-client/test/browser/Utils.jsm", this);
Cu.import("resource://gre/modules/Console.jsm", this);

add_task(Utils.withDriver(Assert, function* uuids(driver) {
  // Test that it is a UUID
  const uuid1 = driver.uuid();
  ok(Utils.UUID_REGEX.test(uuid1), "valid uuid format");

  // Test that UUIDs are different each time
  const uuid2 = driver.uuid();
  isnot(uuid1, uuid2, "uuids are unique");
}));

add_task(Utils.withDriver(Assert, function* userId(driver) {
  // Test that userId is a UUID
  ok(Utils.UUID_REGEX.test(driver.userId), "userId is a uuid");
}));
