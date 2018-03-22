/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Uses the same scope as test_add_actors.js
/* import-globals-from head_dbg.js */

function PostInitTabActor(connection) {}

PostInitTabActor.prototype = {
  actorPostfix: "postInitTab",
  onPing(request) {
    return { message: "pong" };
  },
};

PostInitTabActor.prototype.requestTypes = {
  "ping": PostInitTabActor.prototype.onPing,
};

DebuggerServer.addGlobalActor(PostInitTabActor, "postInitTabActor");
