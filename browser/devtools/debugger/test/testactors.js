/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function TestActor1(aConnection, aTab, aOnDisconnect)
{
  this.conn = aConnection;
  this.tab = aTab;
  this.onDisconnect = aOnDisconnect;
}

TestActor1.prototype = {
  actorPrefix: "testone",

  disconnect: function TA1_disconnect() {
    this.onDisconnect();
  },

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

DebuggerServer.addTabRequest("testTabActor1", function (aTab) {
  if (aTab._testTabActor1) {
    return aTab._testTabActor1.grip();
  }

  let actor = new TestActor1(aTab.conn, aTab.browser, function () {
    delete aTab._testTabActor1;
  });
  aTab.tabActorPool.addActor(actor);
  aTab._testTabActor1 = actor;
  return actor.grip();
});


DebuggerServer.addTabRequest("testContextActor1", function (aTab, aRequest) {
  if (aTab._testContextActor1) {
    return aTab._testContextActor1.grip();
  }

  let actor = new TestActor1(aTab.conn, aTab.browser, function () {
    delete aTab._testContextActor1;
  });
  aTab.contextActorPool.addActor(actor);
  aTab._testContextActor1 = actor;
  return actor.grip();
});

