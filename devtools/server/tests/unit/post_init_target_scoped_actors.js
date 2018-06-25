/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Uses the same scope as test_add_actors.js
/* import-globals-from head_dbg.js */

function PostInitTargetScopedActor(connection) {}

PostInitTargetScopedActor.prototype = {
  actorPostfix: "postInitTab",
  onPing(request) {
    return { message: "pong" };
  },
};

PostInitTargetScopedActor.prototype.requestTypes = {
  "ping": PostInitTargetScopedActor.prototype.onPing,
};

DebuggerServer.addTargetScopedActor({
  constructorName: "PostInitTargetScopedActor",
  constructorFun: PostInitTargetScopedActor,
}, "postInitTargetScopedActor");
