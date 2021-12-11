/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the functionality of the BreakpointActorMap object.

const {
  BreakpointActorMap,
} = require("devtools/server/actors/utils/breakpoint-actor-map");

function run_test() {
  test_get_actor();
  test_set_actor();
  test_delete_actor();
  test_find_actors();
  test_duplicate_actors();
}

function test_get_actor() {
  const bpStore = new BreakpointActorMap();
  const location = {
    generatedSourceActor: { actor: "actor1" },
    generatedLine: 3,
  };
  const columnLocation = {
    generatedSourceActor: { actor: "actor2" },
    generatedLine: 5,
    generatedColumn: 15,
  };

  // Shouldn't have breakpoint
  Assert.equal(
    null,
    bpStore.getActor(location),
    "Breakpoint not added and shouldn't exist."
  );

  bpStore.setActor(location, {});
  Assert.ok(
    !!bpStore.getActor(location),
    "Breakpoint added but not found in Breakpoint Store."
  );

  bpStore.deleteActor(location);
  Assert.equal(
    null,
    bpStore.getActor(location),
    "Breakpoint removed but still exists."
  );

  // Same checks for breakpoint with a column
  Assert.equal(
    null,
    bpStore.getActor(columnLocation),
    "Breakpoint with column not added and shouldn't exist."
  );

  bpStore.setActor(columnLocation, {});
  Assert.ok(
    !!bpStore.getActor(columnLocation),
    "Breakpoint with column added but not found in Breakpoint Store."
  );

  bpStore.deleteActor(columnLocation);
  Assert.equal(
    null,
    bpStore.getActor(columnLocation),
    "Breakpoint with column removed but still exists in Breakpoint Store."
  );
}

function test_set_actor() {
  // Breakpoint with column
  const bpStore = new BreakpointActorMap();
  let location = {
    generatedSourceActor: { actor: "actor1" },
    generatedLine: 10,
    generatedColumn: 9,
  };
  bpStore.setActor(location, {});
  Assert.ok(
    !!bpStore.getActor(location),
    "We should have the column breakpoint we just added"
  );

  // Breakpoint without column (whole line breakpoint)
  location = {
    generatedSourceActor: { actor: "actor2" },
    generatedLine: 103,
  };
  bpStore.setActor(location, {});
  Assert.ok(
    !!bpStore.getActor(location),
    "We should have the whole line breakpoint we just added"
  );
}

function test_delete_actor() {
  // Breakpoint with column
  const bpStore = new BreakpointActorMap();
  let location = {
    generatedSourceActor: { actor: "actor1" },
    generatedLine: 10,
    generatedColumn: 9,
  };
  bpStore.setActor(location, {});
  bpStore.deleteActor(location);
  Assert.equal(
    bpStore.getActor(location),
    null,
    "We should not have the column breakpoint anymore"
  );

  // Breakpoint without column (whole line breakpoint)
  location = {
    generatedSourceActor: { actor: "actor2" },
    generatedLine: 103,
  };
  bpStore.setActor(location, {});
  bpStore.deleteActor(location);
  Assert.equal(
    bpStore.getActor(location),
    null,
    "We should not have the whole line breakpoint anymore"
  );
}

function test_find_actors() {
  const bps = [
    { generatedSourceActor: { actor: "actor1" }, generatedLine: 10 },
    {
      generatedSourceActor: { actor: "actor1" },
      generatedLine: 10,
      generatedColumn: 3,
    },
    {
      generatedSourceActor: { actor: "actor1" },
      generatedLine: 10,
      generatedColumn: 10,
    },
    {
      generatedSourceActor: { actor: "actor1" },
      generatedLine: 23,
      generatedColumn: 89,
    },
    {
      generatedSourceActor: { actor: "actor2" },
      generatedLine: 10,
      generatedColumn: 1,
    },
    {
      generatedSourceActor: { actor: "actor2" },
      generatedLine: 20,
      generatedColumn: 5,
    },
    {
      generatedSourceActor: { actor: "actor2" },
      generatedLine: 30,
      generatedColumn: 34,
    },
    {
      generatedSourceActor: { actor: "actor2" },
      generatedLine: 40,
      generatedColumn: 56,
    },
  ];

  const bpStore = new BreakpointActorMap();

  for (const bp of bps) {
    bpStore.setActor(bp, bp);
  }

  // All breakpoints

  let bpSet = new Set(bps);
  for (const bp of bpStore.findActors()) {
    bpSet.delete(bp);
  }
  Assert.equal(bpSet.size, 0, "Should be able to iterate over all breakpoints");

  // Breakpoints by URL

  bpSet = new Set(
    bps.filter(bp => {
      return bp.generatedSourceActor.actorID === "actor1";
    })
  );
  for (const bp of bpStore.findActors({
    generatedSourceActor: { actorID: "actor1" },
  })) {
    bpSet.delete(bp);
  }
  Assert.equal(bpSet.size, 0, "Should be able to filter the iteration by url");

  // Breakpoints by URL and line

  bpSet = new Set(
    bps.filter(bp => {
      return (
        bp.generatedSourceActor.actorID === "actor1" && bp.generatedLine === 10
      );
    })
  );
  let first = true;
  for (const bp of bpStore.findActors({
    generatedSourceActor: { actorID: "actor1" },
    generatedLine: 10,
  })) {
    if (first) {
      Assert.equal(
        bp.generatedColumn,
        undefined,
        "Should always get the whole line breakpoint first"
      );
      first = false;
    } else {
      Assert.notEqual(
        bp.generatedColumn,
        undefined,
        "Should not get the whole line breakpoint any time other than first."
      );
    }
    bpSet.delete(bp);
  }
  Assert.equal(
    bpSet.size,
    0,
    "Should be able to filter the iteration by url and line"
  );
}

function test_duplicate_actors() {
  const bpStore = new BreakpointActorMap();

  // Breakpoint with column
  let location = {
    generatedSourceActor: { actorID: "foo-actor" },
    generatedLine: 10,
    generatedColumn: 9,
  };
  bpStore.setActor(location, {});
  bpStore.setActor(location, {});
  Assert.equal(bpStore.size, 1, "We should have only 1 column breakpoint");
  bpStore.deleteActor(location);

  // Breakpoint without column (whole line breakpoint)
  location = {
    generatedSourceActor: { actorID: "foo-actor" },
    generatedLine: 15,
  };
  bpStore.setActor(location, {});
  bpStore.setActor(location, {});
  Assert.equal(bpStore.size, 1, "We should have only 1 whole line breakpoint");
  bpStore.deleteActor(location);
}
