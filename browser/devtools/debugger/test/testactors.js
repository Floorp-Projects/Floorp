/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function TestActor1(aConnection, aTab)
{
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

DebuggerServer.removeTabActor(TestActor1);
DebuggerServer.removeGlobalActor(TestActor1);

DebuggerServer.addTabActor(TestActor1, "testTabActor1");
DebuggerServer.addGlobalActor(TestActor1, "testGlobalActor1");
