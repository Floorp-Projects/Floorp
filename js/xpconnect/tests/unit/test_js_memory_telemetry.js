"use strict";

add_task(function test_compartment_realm_counts() {
  const compsSystem = "MEMORY_JS_COMPARTMENTS_SYSTEM";
  const compsUser = "MEMORY_JS_COMPARTMENTS_USER";
  const realmsSystem = "MEMORY_JS_REALMS_SYSTEM";
  const realmsUser = "MEMORY_JS_REALMS_USER";

  Cu.forceShrinkingGC();

  Services.telemetry.gatherMemory();
  let snapshot1 = Services.telemetry.getSnapshotForHistograms("main", true).parent;

  // We can't hard code exact counts, but we can check some basic invariants:
  //
  // * Compartments must contain at least one realm, so there must be more
  //   realms than compartments.
  // * There must be at least one system realm.

  Assert.ok(snapshot1[realmsSystem].sum <= snapshot1[compsSystem].sum,
            "Number of system compartments can't exceed number of system realms");
  Assert.ok(snapshot1[realmsUser].sum <= snapshot1[compsUser].sum,
            "Number of user compartments can't exceed number of user realms");
  Assert.ok(snapshot1[realmsSystem].sum > 0,
            "There must be at least one system realm");

  // Now we create a bunch of sandboxes (more than one to be more resilient
  // against GCs happening in the meantime), so we can check:
  //
  // * There are now more realms and user compartments than before. Not system
  //   compartments, because system realms share a compartment.
  // * The system compartment contains multiple realms.

  let systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
  let arr = [];
  for (let i = 0; i < 5; i++) {
    arr.push(Cu.Sandbox(null));
    arr.push(Cu.Sandbox(systemPrincipal));
  }

  Services.telemetry.gatherMemory();
  let snapshot2 = Services.telemetry.getSnapshotForHistograms("main", true).parent;

  for (let k of [realmsSystem, realmsUser, compsUser]) {
    Assert.ok(snapshot2[k].sum > snapshot1[k].sum,
              "There must be more compartments/realms now: " + k);
  }

  Assert.ok(snapshot2[realmsSystem].sum > snapshot2[compsSystem].sum,
            "There must be more system realms than system compartments now");

  arr[0].x = 10; // Ensure the JS engine keeps |arr| alive until this point.
});
