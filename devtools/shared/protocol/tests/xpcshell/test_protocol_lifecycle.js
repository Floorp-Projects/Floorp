/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol/Actor.js");
const { Front } = require("resource://devtools/shared/protocol/Front.js");

add_task(async function () {
  // Front constructor expect to be provided a client object
  const client = {};
  const front = new Front(client);
  ok(
    !front.isDestroyed(),
    "Blank front with no actor ID is not considered as destroyed"
  );
  front.destroy();
  ok(front.isDestroyed(), "Front is destroyed");

  const actor = new Actor(null, { typeName: "actor", methods: [] });
  ok(
    !actor.isDestroyed(),
    "Blank actor with no actor ID is not considered as destroyed"
  );
  actor.destroy();
  ok(actor.isDestroyed(), "Actor is destroyed");
});
