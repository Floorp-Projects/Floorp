"use strict";

const {utils: Cu} = Components;
Cu.import("resource://shield-recipe-client/lib/NormandyDriver.jsm", this);
Cu.import("resource://shield-recipe-client/lib/SandboxManager.jsm", this);

add_task(function* () {
  const sandboxManager = new SandboxManager();
  sandboxManager.addHold("test running");
  let driver = new NormandyDriver(sandboxManager);

  // Test that UUID look about right
  const uuid1 = driver.uuid();
  ok(/^[a-f0-9-]{36}$/.test(uuid1), "valid uuid format");

  // Test that UUIDs are different each time
  const uuid2 = driver.uuid();
  isnot(uuid1, uuid2, "uuids are unique");

  driver = null;
  sandboxManager.removeHold("test running");

  yield sandboxManager.isNuked()
    .then(() => ok(true, "sandbox is nuked"))
    .catch(e => ok(false, "sandbox is nuked", e));
});
