/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function run_test() {
  // Should get an exception if we try to interact with DebuggerServer
  // before we initialize it...
  Assert.throws(() => DebuggerServer.createListener(),
    /DebuggerServer has not been initialized/,
    "createListener should throw before it has been initialized");
  Assert.throws(DebuggerServer.closeAllListeners,
    /this is undefined/,
    "closeAllListeners should throw before it has been initialized");
  Assert.throws(DebuggerServer.connectPipe,
    /this is undefined/,
    "connectPipe should throw before it has been initialized");

  // Allow incoming connections.
  DebuggerServer.init();

  // These should still fail because we haven't added a createRootActor
  // implementation yet.
  Assert.throws(DebuggerServer.closeAllListeners,
    /this is undefined/,
    "closeAllListeners should throw if createRootActor hasn't been added");
  Assert.throws(DebuggerServer.connectPipe,
    /this is undefined/,
    "closeAllListeners should throw if createRootActor hasn't been added");

  const { createRootActor } = require("xpcshell-test/testactors");
  DebuggerServer.setRootActor(createRootActor);

  // Now they should work.
  DebuggerServer.createListener();
  DebuggerServer.closeAllListeners();

  // Make sure we got the test's root actor all set up.
  const client1 = DebuggerServer.connectPipe();
  client1.hooks = {
    onPacket: function(packet1) {
      Assert.equal(packet1.from, "root");
      Assert.equal(packet1.applicationType, "xpcshell-tests");

      // Spin up a second connection, make sure it has its own root
      // actor.
      const client2 = DebuggerServer.connectPipe();
      client2.hooks = {
        onPacket: function(packet2) {
          Assert.equal(packet2.from, "root");
          Assert.notEqual(packet1.testConnectionPrefix,
                          packet2.testConnectionPrefix);
          client2.close();
        },
        onClosed: function(result) {
          client1.close();
        },
      };
      client2.ready();
    },

    onClosed: function(result) {
      do_test_finished();
    },
  };

  client1.ready();
  do_test_pending();
}
