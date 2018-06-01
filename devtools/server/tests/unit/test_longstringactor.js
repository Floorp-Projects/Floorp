/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { LongStringActor } = require("devtools/server/actors/object/long-string");

function run_test() {
  test_LSA_destroy();
  test_LSA_grip();
  test_LSA_onSubstring();
}

const TEST_STRING = "This is a very long string!";

function makeMockLongStringActor() {
  const string = TEST_STRING;
  const actor = new LongStringActor(string);
  actor.actorID = "longString1";
  actor.registeredPool = {
    longStringActors: {
      [string]: actor
    }
  };
  return actor;
}

function test_LSA_destroy() {
  const actor = makeMockLongStringActor();
  Assert.equal(actor.registeredPool.longStringActors[TEST_STRING], actor);

  actor.destroy();
  Assert.equal(actor.registeredPool.longStringActors[TEST_STRING], void 0);
}

function test_LSA_grip() {
  const actor = makeMockLongStringActor();

  const grip = actor.grip();
  Assert.equal(grip.type, "longString");
  Assert.equal(grip.initial,
               TEST_STRING.substring(0, DebuggerServer.LONG_STRING_INITIAL_LENGTH));
  Assert.equal(grip.length, TEST_STRING.length);
  Assert.equal(grip.actor, actor.actorID);
}

function test_LSA_onSubstring() {
  const actor = makeMockLongStringActor();
  let response;

  // From the start
  response = actor.onSubstring({
    start: 0,
    end: 4
  });
  Assert.equal(response.from, actor.actorID);
  Assert.equal(response.substring, TEST_STRING.substring(0, 4));

  // In the middle
  response = actor.onSubstring({
    start: 5,
    end: 8
  });
  Assert.equal(response.from, actor.actorID);
  Assert.equal(response.substring, TEST_STRING.substring(5, 8));

  // Whole string
  response = actor.onSubstring({
    start: 0,
    end: TEST_STRING.length
  });
  Assert.equal(response.from, actor.actorID);
  Assert.equal(response.substring, TEST_STRING);

  // Negative index
  response = actor.onSubstring({
    start: -5,
    end: TEST_STRING.length
  });
  Assert.equal(response.from, actor.actorID);
  Assert.equal(response.substring,
               TEST_STRING.substring(-5, TEST_STRING.length));

  // Past the end
  response = actor.onSubstring({
    start: TEST_STRING.length - 5,
    end: 100
  });
  Assert.equal(response.from, actor.actorID);
  Assert.equal(response.substring,
               TEST_STRING.substring(TEST_STRING.length - 5, 100));
}
