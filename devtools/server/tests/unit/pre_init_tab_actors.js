/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

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
