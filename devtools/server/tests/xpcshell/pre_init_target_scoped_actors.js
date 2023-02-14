/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol/Actor.js");

class PreInitTargetScopedActor extends Actor {
  constructor(conn) {
    super(conn, { typeName: "preInitTargetScoped", methods: [] });

    this.requestTypes = {
      ping: this.onPing,
    };
  }

  onPing() {
    return { message: "pong" };
  }
}

exports.PreInitTargetScopedActor = PreInitTargetScopedActor;
