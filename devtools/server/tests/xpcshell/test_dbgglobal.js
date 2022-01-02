/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { SocketListener } = require("devtools/shared/security/socket");

function run_test() {
  // Should get an exception if we try to interact with DevToolsServer
  // before we initialize it...
  const socketListener = new SocketListener(DevToolsServer, {});
  Assert.throws(
    () => DevToolsServer.addSocketListener(socketListener),
    /DevToolsServer has not been initialized/,
    "addSocketListener should throw before it has been initialized"
  );
  Assert.throws(
    DevToolsServer.closeAllSocketListeners,
    /this is undefined/,
    "closeAllSocketListeners should throw before it has been initialized"
  );
  Assert.throws(
    DevToolsServer.connectPipe,
    /this is undefined/,
    "connectPipe should throw before it has been initialized"
  );

  // Allow incoming connections.
  DevToolsServer.init();

  // These should still fail because we haven't added a createRootActor
  // implementation yet.
  Assert.throws(
    DevToolsServer.closeAllSocketListeners,
    /this is undefined/,
    "closeAllSocketListeners should throw if createRootActor hasn't been added"
  );
  Assert.throws(
    DevToolsServer.connectPipe,
    /this is undefined/,
    "closeAllSocketListeners should throw if createRootActor hasn't been added"
  );

  const { createRootActor } = require("xpcshell-test/testactors");
  DevToolsServer.setRootActor(createRootActor);

  // Now they should work.
  DevToolsServer.addSocketListener(socketListener);
  DevToolsServer.closeAllSocketListeners();

  // Make sure we got the test's root actor all set up.
  const client1 = DevToolsServer.connectPipe();
  client1.hooks = {
    onPacket: function(packet1) {
      Assert.equal(packet1.from, "root");
      Assert.equal(packet1.applicationType, "xpcshell-tests");

      // Spin up a second connection, make sure it has its own root
      // actor.
      const client2 = DevToolsServer.connectPipe();
      client2.hooks = {
        onPacket: function(packet2) {
          Assert.equal(packet2.from, "root");
          Assert.notEqual(
            packet1.testConnectionPrefix,
            packet2.testConnectionPrefix
          );
          client2.close();
        },
        onTransportClosed: function(result) {
          client1.close();
        },
      };
      client2.ready();
    },

    onTransportClosed: function(result) {
      do_test_finished();
    },
  };

  client1.ready();
  do_test_pending();
}
