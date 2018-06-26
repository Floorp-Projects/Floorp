/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function TestActor1(aConnection, aTab) {
  this.conn = aConnection;
  this.tab = aTab;
}

TestActor1.prototype = {
  actorPrefix: "test_one",

  grip: function TA1_grip() {
    return { actor: this.actorID,
             test: "TestActor1" };
  },

  onPing: function TA1_onPing() {
    return { pong: "pong" };
  }
};

TestActor1.prototype.requestTypes = {
  "ping": TestActor1.prototype.onPing
};

DebuggerServer.removeTargetScopedActor("testTargetScopedActor1");
DebuggerServer.removeGlobalActor("testGlobalActor1");

DebuggerServer.addTargetScopedActor({
  constructorName: "TestActor1",
  constructorFun: TestActor1,
}, "testTargetScopedActor1");
DebuggerServer.addGlobalActor({
  constructorName: "TestActor1",
  constructorFun: TestActor1,
}, "testGlobalActor1");
