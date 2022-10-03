/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Actor } = require("resource://devtools/shared/protocol/Actor.js");

class PreInitGlobalActor extends Actor {
  constructor(conn) {
    super(conn);

    this.typeName = "preInitGlobal";
    this.requestTypes = {
      ping: this.onPing,
    };
  }

  onPing() {
    return { message: "pong" };
  }
}

exports.PreInitGlobalActor = PreInitGlobalActor;
