/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Uses the same scope as test_add_actors.js
/* import-globals-from head_dbg.js */

function PreInitGlobalActor(connection) {}

PreInitGlobalActor.prototype = {
  actorPrefix: "preInitGlobal",
  onPing(request) {
    return { message: "pong" };
  },
};

PreInitGlobalActor.prototype.requestTypes = {
  "ping": PreInitGlobalActor.prototype.onPing,
};

DebuggerServer.addGlobalActor({
  constructorName: "PreInitGlobalActor",
  constructorFun: PreInitGlobalActor,
}, "preInitGlobalActor");
