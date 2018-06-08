/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Uses the same scope as test_add_actors.js
/* import-globals-from head_dbg.js */

function PostInitGlobalActor(connection) {}

PostInitGlobalActor.prototype = {
  actorPrefix: "postInitGlobal",
  onPing(request) {
    return { message: "pong" };
  },
};

PostInitGlobalActor.prototype.requestTypes = {
  "ping": PostInitGlobalActor.prototype.onPing,
};

DebuggerServer.addGlobalActor({
  constructorName: "PostInitGlobalActor",
  constructorFun: PostInitGlobalActor,
}, "postInitGlobalActor");
