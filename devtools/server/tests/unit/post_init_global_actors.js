/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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

DebuggerServer.addGlobalActor(PostInitGlobalActor, "postInitGlobalActor");
