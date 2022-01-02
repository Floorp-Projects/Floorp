/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that a NodeListActor initialized with null nodelist doesn't cause
// exceptions when calling NodeListActor.form.

const { NodeListActor } = require("devtools/server/actors/inspector/node");

function run_test() {
  check_actor_for_list(null);
  check_actor_for_list([]);
  check_actor_for_list(["fakenode"]);
}

function check_actor_for_list(nodelist) {
  info("Checking NodeListActor with nodelist '" + nodelist + "' works.");
  const actor = new NodeListActor({}, nodelist);
  const form = actor.form();

  // No exception occured as a exceptions abort the test.
  ok(true, "No exceptions occured.");
  equal(
    form.length,
    nodelist ? nodelist.length : 0,
    "NodeListActor reported correct length."
  );
}
