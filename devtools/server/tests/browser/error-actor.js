/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global DebuggerServer */

/**
 * Test actor designed to check that clients are properly notified of errors when calling
 * methods on old style actors.
 */
function ErrorActor(conn, tab) {
  this.conn = conn;
  this.tab = tab;
}

ErrorActor.prototype = {
  actorPrefix: "error",

  onError: function() {
    throw new Error("error");
  }
};

ErrorActor.prototype.requestTypes = {
  "error": ErrorActor.prototype.onError
};

DebuggerServer.removeGlobalActor("errorActor");
DebuggerServer.addGlobalActor({
  constructorName: "ErrorActor",
  constructorFun: ErrorActor,
}, "errorActor");
