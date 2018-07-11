/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Uses the same scope as test_add_actors.js
/* import-globals-from head_dbg.js */

function PreInitTargetScopedActor(connection) {}

PreInitTargetScopedActor.prototype = {
  actorPrefix: "preInitTargetScoped",
  onPing(request) {
    return { message: "pong" };
  },
};

PreInitTargetScopedActor.prototype.requestTypes = {
  "ping": PreInitTargetScopedActor.prototype.onPing,
};
exports.PreInitTargetScopedActor = PreInitTargetScopedActor;
