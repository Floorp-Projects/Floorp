/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Bug 755412 - checks if the server drops the connection on an improperly
 * framed packet, i.e. when the length header is invalid.
 */
"use strict";

const { RawPacket } = require("devtools/shared/transport/packets");

function run_test() {
  info("Starting test at " + new Date().toTimeString());
  initTestDebuggerServer();

  add_task(test_socket_conn_drops_after_invalid_header);
  add_task(test_socket_conn_drops_after_invalid_header_2);
  add_task(test_socket_conn_drops_after_too_large_length);
  add_task(test_socket_conn_drops_after_too_long_header);
  run_next_test();
}

function test_socket_conn_drops_after_invalid_header() {
  return test_helper('fluff30:27:{"to":"root","type":"echo"}');
}

function test_socket_conn_drops_after_invalid_header_2() {
  return test_helper('27asd:{"to":"root","type":"echo"}');
}

function test_socket_conn_drops_after_too_large_length() {
  // Packet length is limited (semi-arbitrarily) to 1 TiB (2^40)
  return test_helper("4305724038957487634549823475894325:");
}

function test_socket_conn_drops_after_too_long_header() {
  // The packet header is currently limited to no more than 200 bytes
  let rawPacket = "4305724038957487634549823475894325";
  for (let i = 0; i < 8; i++) {
    rawPacket += rawPacket;
  }
  return test_helper(rawPacket + ":");
}

var test_helper = Task.async(function* (payload) {
  let AuthenticatorType = DebuggerServer.Authenticators.get("PROMPT");
  let authenticator = new AuthenticatorType.Server();
  authenticator.allowConnection = () => {
    return DebuggerServer.AuthenticationResult.ALLOW;
  };

  let listener = DebuggerServer.createListener();
  listener.portOrPath = -1;
  listener.authenticator = authenticator;
  listener.open();

  let transport = yield DebuggerClient.socketConnect({
    host: "127.0.0.1",
    port: listener.port
  });
  let closedDeferred = defer();
  transport.hooks = {
    onPacket: function (packet) {
      this.onPacket = function () {
        do_throw(new Error("This connection should be dropped."));
        transport.close();
      };

      // Inject the payload directly into the stream.
      transport._outgoing.push(new RawPacket(transport, payload));
      transport._flushOutgoing();
    },
    onClosed: function (status) {
      Assert.ok(true);
      closedDeferred.resolve();
    },
  };
  transport.ready();
  return closedDeferred.promise;
});
