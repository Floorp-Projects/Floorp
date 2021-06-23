/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Actor } = require("devtools/shared/protocol/Actor");
const { Front } = require("devtools/shared/protocol/Front");

add_task(async function() {
  const front = new Front();
  ok(
    !front.isDestroyed(),
    "Blank front with no actor ID is not considered as destroyed"
  );
  front.destroy();
  ok(front.isDestroyed(), "Front is destroyed");

  const actor = new Actor();
  ok(
    !actor.isDestroyed(),
    "Blank actor with no actor ID is not considered as destroyed"
  );
  actor.destroy();
  ok(actor.isDestroyed(), "Actor is destroyed");
});
