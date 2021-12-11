/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Actor } = require("devtools/shared/protocol/Actor");

/**
 * Test actor designed to check that clients are properly notified of errors when calling
 * methods on old style actors.
 */
class ErrorActor extends Actor {
  constructor(conn, tab) {
    super(conn);
    this.tab = tab;
    this.typeName = "error";
    this.requestTypes = {
      error: this.onError,
    };
  }
  onError() {
    throw new Error("error");
  }
}

exports.ErrorActor = ErrorActor;
