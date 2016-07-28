/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that JITOptimizations create OptimizationSites, and the underlying
 * hasSuccessfulOutcome/isSuccessfulOutcome work as intended.
 */

function run_test() {
  run_next_test();
}

add_task(function test() {
  let {
    JITOptimizations, hasSuccessfulOutcome, isSuccessfulOutcome, SUCCESSFUL_OUTCOMES
  } = require("devtools/client/performance/modules/logic/jit");

  let rawSites = [];
  rawSites.push(gRawSite2);
  rawSites.push(gRawSite2);
  rawSites.push(gRawSite1);
  rawSites.push(gRawSite1);
  rawSites.push(gRawSite2);
  rawSites.push(gRawSite3);

  let jit = new JITOptimizations(rawSites, gStringTable.stringTable);
  let sites = jit.optimizationSites;

  let [first, second, third] = sites;

  /* hasSuccessfulOutcome */
  equal(hasSuccessfulOutcome(first), false,
        "hasSuccessfulOutcome() returns expected (1)");
  equal(hasSuccessfulOutcome(second), true,
        "hasSuccessfulOutcome() returns expected (2)");
  equal(hasSuccessfulOutcome(third), true,
        "hasSuccessfulOutcome() returns expected (3)");

  /* .data.attempts */
  equal(first.data.attempts.length, 2,
        "optSite.data.attempts has the correct amount of attempts (1)");
  equal(second.data.attempts.length, 5,
        "optSite.data.attempts has the correct amount of attempts (2)");
  equal(third.data.attempts.length, 3,
        "optSite.data.attempts has the correct amount of attempts (3)");

  /* .data.types */
  equal(first.data.types.length, 1,
        "optSite.data.types has the correct amount of IonTypes (1)");
  equal(second.data.types.length, 2,
        "optSite.data.types has the correct amount of IonTypes (2)");
  equal(third.data.types.length, 1,
        "optSite.data.types has the correct amount of IonTypes (3)");

  /* isSuccessfulOutcome */
  ok(SUCCESSFUL_OUTCOMES.length, "Have some successful outcomes in SUCCESSFUL_OUTCOMES");
  SUCCESSFUL_OUTCOMES.forEach(outcome =>
    ok(isSuccessfulOutcome(outcome),
      `${outcome} considered a successful outcome via isSuccessfulOutcome()`));
});

var gStringTable = new RecordingUtils.UniqueStrings();

function uniqStr(s) {
  return gStringTable.getOrAddStringIndex(s);
}

var gRawSite1 = {
  line: 12,
  column: 2,
  types: [{
    mirType: uniqStr("Object"),
    site: uniqStr("A (http://foo/bar/bar:12)"),
    typeset: [{
      keyedBy: uniqStr("constructor"),
      name: uniqStr("Foo"),
      location: uniqStr("A (http://foo/bar/baz:12)")
    }, {
      keyedBy: uniqStr("constructor"),
      location: uniqStr("A (http://foo/bar/baz:12)")
    }]
  }, {
    mirType: uniqStr("Int32"),
    site: uniqStr("A (http://foo/bar/bar:12)"),
    typeset: [{
      keyedBy: uniqStr("primitive"),
      location: uniqStr("self-hosted")
    }]
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")],
      [uniqStr("Inlined"), uniqStr("SomeGetter3")]
    ]
  }
};

var gRawSite2 = {
  line: 34,
  types: [{
    mirType: uniqStr("Int32"),
    site: uniqStr("Receiver")
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")]
    ]
  }
};

var gRawSite3 = {
  line: 78,
  types: [{
    mirType: uniqStr("Object"),
    site: uniqStr("A (http://foo/bar/bar:12)"),
    typeset: [{
      keyedBy: uniqStr("constructor"),
      name: uniqStr("Foo"),
      location: uniqStr("A (http://foo/bar/baz:12)")
    }, {
      keyedBy: uniqStr("primitive"),
      location: uniqStr("self-hosted")
    }]
  }],
  attempts: {
    schema: {
      outcome: 0,
      strategy: 1
    },
    data: [
      [uniqStr("Failure1"), uniqStr("SomeGetter1")],
      [uniqStr("Failure2"), uniqStr("SomeGetter2")],
      [uniqStr("GenericSuccess"), uniqStr("SomeGetter3")]
    ]
  }
};
