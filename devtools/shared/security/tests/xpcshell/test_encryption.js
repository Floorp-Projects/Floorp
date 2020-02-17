/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test basic functionality of DevTools client and server TLS encryption mode
function run_test() {
  // Need profile dir to store the key / cert
  do_get_profile();
  // Ensure PSM is initialized
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);
  run_next_test();
}

function connectClient(client) {
  return client.connect();
}

add_task(async function() {
  initTestDevToolsServer();
});

// Client w/ encryption connects successfully to server w/ encryption
add_task(async function() {
  equal(DevToolsServer.listeningSockets, 0, "0 listening sockets");

  const AuthenticatorType = DevToolsServer.Authenticators.get("PROMPT");
  const authenticator = new AuthenticatorType.Server();
  authenticator.allowConnection = () => {
    return DevToolsServer.AuthenticationResult.ALLOW;
  };

  const socketOptions = {
    authenticator,
    encryption: true,
    portOrPath: -1,
  };
  const listener = new SocketListener(DevToolsServer, socketOptions);
  ok(listener, "Socket listener created");
  await listener.open();
  equal(DevToolsServer.listeningSockets, 1, "1 listening socket");

  const transport = await DevToolsClient.socketConnect({
    host: "127.0.0.1",
    port: listener.port,
    encryption: true,
  });
  ok(transport, "Client transport created");

  const client = new DevToolsClient(transport);
  const onUnexpectedClose = () => {
    do_throw("Closed unexpectedly");
  };
  client.on("closed", onUnexpectedClose);
  await connectClient(client);

  // Send a message the server will echo back
  const message = "secrets";
  const reply = await client.mainRoot.echo({ message });
  equal(reply.message, message, "Encrypted echo matches");

  client.off("closed", onUnexpectedClose);
  transport.close();
  listener.close();
  equal(DevToolsServer.listeningSockets, 0, "0 listening sockets");
});

// Client w/o encryption fails to connect to server w/ encryption
add_task(async function() {
  equal(DevToolsServer.listeningSockets, 0, "0 listening sockets");

  const AuthenticatorType = DevToolsServer.Authenticators.get("PROMPT");
  const authenticator = new AuthenticatorType.Server();
  authenticator.allowConnection = () => {
    return DevToolsServer.AuthenticationResult.ALLOW;
  };

  const socketOptions = {
    authenticator,
    encryption: true,
    portOrPath: -1,
  };
  const listener = new SocketListener(DevToolsServer, socketOptions);
  ok(listener, "Socket listener created");
  await listener.open();
  equal(DevToolsServer.listeningSockets, 1, "1 listening socket");

  try {
    await DevToolsClient.socketConnect({
      host: "127.0.0.1",
      port: listener.port,
      // encryption: false is the default
    });
  } catch (e) {
    ok(true, "Client failed to connect as expected");
    listener.close();
    equal(DevToolsServer.listeningSockets, 0, "0 listening sockets");
    return;
  }

  do_throw("Connection unexpectedly succeeded");
});

add_task(async function() {
  DevToolsServer.destroy();
});
