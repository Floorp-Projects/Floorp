/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Uses the same scope as test_add_actors.js
/* import-globals-from head_dbg.js */

function PreInitTabActor(connection) {}

PreInitTabActor.prototype = {
  actorPrefix: "preInitTab",
  onPing(request) {
    return { message: "pong" };
  },
};

PreInitTabActor.prototype.requestTypes = {
  "ping": PreInitTabActor.prototype.onPing,
};

DebuggerServer.addGlobalActor(PreInitTabActor, "preInitTabActor");
