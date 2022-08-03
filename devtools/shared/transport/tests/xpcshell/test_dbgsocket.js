/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var gPort;
var gExtraListener;

function run_test() {
  info("Starting test at " + new Date().toTimeString());
  initTestDevToolsServer();

  add_task(test_socket_conn);
  add_task(test_socket_shutdown);
  add_test(test_pipe_conn);

  run_next_test();
}

async function test_socket_conn() {
  Assert.equal(DevToolsServer.listeningSockets, 0);
  const AuthenticatorType = DevToolsServer.Authenticators.get("PROMPT");
  const authenticator = new AuthenticatorType.Server();
  authenticator.allowConnection = () => {
    return DevToolsServer.AuthenticationResult.ALLOW;
  };
  const socketOptions = {
    authenticator,
    portOrPath: -1,
  };
  const listener = new SocketListener(DevToolsServer, socketOptions);
  Assert.ok(listener);
  listener.open();
  Assert.equal(DevToolsServer.listeningSockets, 1);
  gPort = DevToolsServer._listeners[0].port;
  info("DevTools server port is " + gPort);
  // Open a second, separate listener
  gExtraListener = new SocketListener(DevToolsServer, socketOptions);
  gExtraListener.open();
  Assert.equal(DevToolsServer.listeningSockets, 2);
  Assert.ok(!DevToolsServer.hasConnection());

  info("Starting long and unicode tests at " + new Date().toTimeString());
  const unicodeString = "(╯°□°）╯︵ ┻━┻";
  const transport = await DevToolsClient.socketConnect({
    host: "127.0.0.1",
    port: gPort,
  });
  Assert.ok(DevToolsServer.hasConnection());

  // Assert that connection settings are available on transport object
  const settings = transport.connectionSettings;
  Assert.equal(settings.host, "127.0.0.1");
  Assert.equal(settings.port, gPort);

  const onDebuggerConnectionClosed = DevToolsServer.once("connectionchange");
  await new Promise(resolve => {
    transport.hooks = {
      onPacket(packet) {
        this.onPacket = function({ unicode }) {
          Assert.equal(unicode, unicodeString);
          transport.close();
        };
        // Verify that things work correctly when bigger than the output
        // transport buffers and when transporting unicode...
        transport.send({
          to: "root",
          type: "echo",
          reallylong: really_long(),
          unicode: unicodeString,
        });
        Assert.equal(packet.from, "root");
      },
      onTransportClosed(status) {
        resolve();
      },
    };
    transport.ready();
  });
  const type = await onDebuggerConnectionClosed;
  Assert.equal(type, "closed");
  Assert.ok(!DevToolsServer.hasConnection());
}

async function test_socket_shutdown() {
  Assert.equal(DevToolsServer.listeningSockets, 2);
  gExtraListener.close();
  Assert.equal(DevToolsServer.listeningSockets, 1);
  Assert.ok(DevToolsServer.closeAllSocketListeners());
  Assert.equal(DevToolsServer.listeningSockets, 0);
  // Make sure closing the listener twice does nothing.
  Assert.ok(!DevToolsServer.closeAllSocketListeners());
  Assert.equal(DevToolsServer.listeningSockets, 0);

  info("Connecting to a server socket at " + new Date().toTimeString());
  try {
    await DevToolsClient.socketConnect({
      host: "127.0.0.1",
      port: gPort,
    });
  } catch (e) {
    if (
      e.result == Cr.NS_ERROR_CONNECTION_REFUSED ||
      e.result == Cr.NS_ERROR_NET_TIMEOUT
    ) {
      // The connection should be refused here, but on slow or overloaded
      // machines it may just time out.
      Assert.ok(true);
      return;
    }
    throw e;
  }

  // Shouldn't reach this, should never connect.
  Assert.ok(false);
}

function test_pipe_conn() {
  const transport = DevToolsServer.connectPipe();
  transport.hooks = {
    onPacket(packet) {
      Assert.equal(packet.from, "root");
      transport.close();
    },
    onTransportClosed(status) {
      run_next_test();
    },
  };

  transport.ready();
}
