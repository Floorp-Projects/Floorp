/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

var gPort;
var gExtraListener;

function run_test() {
  info("Starting test at " + new Date().toTimeString());
  initTestDebuggerServer();

  add_task(test_socket_conn);
  add_task(test_socket_shutdown);
  add_test(test_pipe_conn);

  run_next_test();
}

async function test_socket_conn() {
  Assert.equal(DebuggerServer.listeningSockets, 0);
  const AuthenticatorType = DebuggerServer.Authenticators.get("PROMPT");
  const authenticator = new AuthenticatorType.Server();
  authenticator.allowConnection = () => {
    return DebuggerServer.AuthenticationResult.ALLOW;
  };
  const listener = DebuggerServer.createListener();
  Assert.ok(listener);
  listener.portOrPath = -1;
  listener.authenticator = authenticator;
  listener.open();
  Assert.equal(DebuggerServer.listeningSockets, 1);
  gPort = DebuggerServer._listeners[0].port;
  info("Debugger server port is " + gPort);
  // Open a second, separate listener
  gExtraListener = DebuggerServer.createListener();
  gExtraListener.portOrPath = -1;
  gExtraListener.authenticator = authenticator;
  gExtraListener.open();
  Assert.equal(DebuggerServer.listeningSockets, 2);

  info("Starting long and unicode tests at " + new Date().toTimeString());
  const unicodeString = "(╯°□°）╯︵ ┻━┻";
  const transport = await DebuggerClient.socketConnect({
    host: "127.0.0.1",
    port: gPort
  });

  // Assert that connection settings are available on transport object
  const settings = transport.connectionSettings;
  Assert.equal(settings.host, "127.0.0.1");
  Assert.equal(settings.port, gPort);

  const closedDeferred = defer();
  transport.hooks = {
    onPacket: function(packet) {
      this.onPacket = function({unicode}) {
        Assert.equal(unicode, unicodeString);
        transport.close();
      };
      // Verify that things work correctly when bigger than the output
      // transport buffers and when transporting unicode...
      transport.send({to: "root",
                      type: "echo",
                      reallylong: really_long(),
                      unicode: unicodeString});
      Assert.equal(packet.from, "root");
    },
    onClosed: function(status) {
      closedDeferred.resolve();
    },
  };
  transport.ready();
  return closedDeferred.promise;
}

async function test_socket_shutdown() {
  Assert.equal(DebuggerServer.listeningSockets, 2);
  gExtraListener.close();
  Assert.equal(DebuggerServer.listeningSockets, 1);
  Assert.ok(DebuggerServer.closeAllListeners());
  Assert.equal(DebuggerServer.listeningSockets, 0);
  // Make sure closing the listener twice does nothing.
  Assert.ok(!DebuggerServer.closeAllListeners());
  Assert.equal(DebuggerServer.listeningSockets, 0);

  info("Connecting to a server socket at " + new Date().toTimeString());
  try {
    await DebuggerClient.socketConnect({
      host: "127.0.0.1",
      port: gPort
    });
  } catch (e) {
    if (e.result == Cr.NS_ERROR_CONNECTION_REFUSED ||
        e.result == Cr.NS_ERROR_NET_TIMEOUT) {
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
  const transport = DebuggerServer.connectPipe();
  transport.hooks = {
    onPacket: function(packet) {
      Assert.equal(packet.from, "root");
      transport.close();
    },
    onClosed: function(status) {
      run_next_test();
    }
  };

  transport.ready();
}
