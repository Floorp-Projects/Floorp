/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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

DebuggerServer.addGlobalActor(PreInitGlobalActor, "preInitGlobalActor");
