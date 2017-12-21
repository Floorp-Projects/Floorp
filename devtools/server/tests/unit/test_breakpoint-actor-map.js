/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test the functionality of the BreakpointActorMap object.

const { BreakpointActorMap } = require("devtools/server/actors/script");

function run_test() {
  test_get_actor();
  test_set_actor();
  test_delete_actor();
  test_find_actors();
  test_duplicate_actors();
}

function test_get_actor() {
  let bpStore = new BreakpointActorMap();
  let location = {
    originalSourceActor: { actor: "actor1" },
    originalLine: 3
  };
  let columnLocation = {
    originalSourceActor: { actor: "actor2" },
    originalLine: 5,
    originalColumn: 15
  };

  // Shouldn't have breakpoint
  Assert.equal(null, bpStore.getActor(location),
               "Breakpoint not added and shouldn't exist.");

  bpStore.setActor(location, {});
  Assert.ok(!!bpStore.getActor(location),
            "Breakpoint added but not found in Breakpoint Store.");

  bpStore.deleteActor(location);
  Assert.equal(null, bpStore.getActor(location),
               "Breakpoint removed but still exists.");

  // Same checks for breakpoint with a column
  Assert.equal(null, bpStore.getActor(columnLocation),
               "Breakpoint with column not added and shouldn't exist.");

  bpStore.setActor(columnLocation, {});
  Assert.ok(!!bpStore.getActor(columnLocation),
            "Breakpoint with column added but not found in Breakpoint Store.");

  bpStore.deleteActor(columnLocation);
  Assert.equal(null, bpStore.getActor(columnLocation),
               "Breakpoint with column removed but still exists in Breakpoint Store.");
}

function test_set_actor() {
  // Breakpoint with column
  let bpStore = new BreakpointActorMap();
  let location = {
    originalSourceActor: { actor: "actor1" },
    originalLine: 10,
    originalColumn: 9
  };
  bpStore.setActor(location, {});
  Assert.ok(!!bpStore.getActor(location),
            "We should have the column breakpoint we just added");

  // Breakpoint without column (whole line breakpoint)
  location = {
    originalSourceActor: { actor: "actor2" },
    originalLine: 103
  };
  bpStore.setActor(location, {});
  Assert.ok(!!bpStore.getActor(location),
            "We should have the whole line breakpoint we just added");
}

function test_delete_actor() {
  // Breakpoint with column
  let bpStore = new BreakpointActorMap();
  let location = {
    originalSourceActor: { actor: "actor1" },
    originalLine: 10,
    originalColumn: 9
  };
  bpStore.setActor(location, {});
  bpStore.deleteActor(location);
  Assert.equal(bpStore.getActor(location), null,
               "We should not have the column breakpoint anymore");

  // Breakpoint without column (whole line breakpoint)
  location = {
    originalSourceActor: { actor: "actor2" },
    originalLine: 103
  };
  bpStore.setActor(location, {});
  bpStore.deleteActor(location);
  Assert.equal(bpStore.getActor(location), null,
               "We should not have the whole line breakpoint anymore");
}

function test_find_actors() {
  let bps = [
    { originalSourceActor: { actor: "actor1" }, originalLine: 10 },
    { originalSourceActor: { actor: "actor1" }, originalLine: 10, originalColumn: 3 },
    { originalSourceActor: { actor: "actor1" }, originalLine: 10, originalColumn: 10 },
    { originalSourceActor: { actor: "actor1" }, originalLine: 23, originalColumn: 89 },
    { originalSourceActor: { actor: "actor2" }, originalLine: 10, originalColumn: 1 },
    { originalSourceActor: { actor: "actor2" }, originalLine: 20, originalColumn: 5 },
    { originalSourceActor: { actor: "actor2" }, originalLine: 30, originalColumn: 34 },
    { originalSourceActor: { actor: "actor2" }, originalLine: 40, originalColumn: 56 }
  ];

  let bpStore = new BreakpointActorMap();

  for (let bp of bps) {
    bpStore.setActor(bp, bp);
  }

  // All breakpoints

  let bpSet = new Set(bps);
  for (let bp of bpStore.findActors()) {
    bpSet.delete(bp);
  }
  Assert.equal(bpSet.size, 0,
               "Should be able to iterate over all breakpoints");

  // Breakpoints by URL

  bpSet = new Set(bps.filter(bp => {
    return bp.originalSourceActor.actorID === "actor1";
  }));
  for (let bp of bpStore.findActors({ originalSourceActor: { actorID: "actor1" } })) {
    bpSet.delete(bp);
  }
  Assert.equal(bpSet.size, 0,
               "Should be able to filter the iteration by url");

  // Breakpoints by URL and line

  bpSet = new Set(bps.filter(bp => {
    return bp.originalSourceActor.actorID === "actor1" && bp.originalLine === 10;
  }));
  let first = true;
  for (let bp of bpStore.findActors({ originalSourceActor: { actorID: "actor1" },
                                      originalLine: 10 })) {
    if (first) {
      Assert.equal(bp.originalColumn, undefined,
                   "Should always get the whole line breakpoint first");
      first = false;
    } else {
      Assert.notEqual(bp.originalColumn, undefined,
                      "Should not get the whole line breakpoint any time other than first.");
    }
    bpSet.delete(bp);
  }
  Assert.equal(bpSet.size, 0,
               "Should be able to filter the iteration by url and line");
}

function test_duplicate_actors() {
  let bpStore = new BreakpointActorMap();

  // Breakpoint with column
  let location = {
    originalSourceActor: { actorID: "foo-actor" },
    originalLine: 10,
    originalColumn: 9
  };
  bpStore.setActor(location, {});
  bpStore.setActor(location, {});
  Assert.equal(bpStore.size, 1, "We should have only 1 column breakpoint");
  bpStore.deleteActor(location);

  // Breakpoint without column (whole line breakpoint)
  location = {
    originalSourceActor: { actorID: "foo-actor" },
    originalLine: 15
  };
  bpStore.setActor(location, {});
  bpStore.setActor(location, {});
  Assert.equal(bpStore.size, 1, "We should have only 1 whole line breakpoint");
  bpStore.deleteActor(location);
}
