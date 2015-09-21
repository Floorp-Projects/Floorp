/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var cert = require("devtools/shared/security/cert");

// Test basic functionality of DevTools client and server OOB_CERT auth (used
// with WiFi debugging)
function run_test() {
  // Need profile dir to store the key / cert
  do_get_profile();
  // Ensure PSM is initialized
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);
  run_next_test();
}

function connectClient(client) {
  let deferred = promise.defer();
  client.connect(() => {
    client.listTabs(deferred.resolve);
  });
  return deferred.promise;
}

add_task(function*() {
  initTestDebuggerServer();
});

// Client w/ OOB_CERT auth connects successfully to server w/ OOB_CERT auth
add_task(function*() {
  equal(DebuggerServer.listeningSockets, 0, "0 listening sockets");

  // Grab our cert, instead of relying on a discovery advertisement
  let serverCert = yield cert.local.getOrCreate();

  let oobData = promise.defer();
  let AuthenticatorType = DebuggerServer.Authenticators.get("OOB_CERT");
  let serverAuth = new AuthenticatorType.Server();
  serverAuth.allowConnection = () => {
    return DebuggerServer.AuthenticationResult.ALLOW;
  };
  serverAuth.receiveOOB = () => oobData.promise; // Skip prompt for tests

  let listener = DebuggerServer.createListener();
  ok(listener, "Socket listener created");
  listener.portOrPath = -1 /* any available port */;
  listener.encryption = true /* required for OOB_CERT */;
  listener.authenticator = serverAuth;
  yield listener.open();
  equal(DebuggerServer.listeningSockets, 1, "1 listening socket");

  let clientAuth = new AuthenticatorType.Client();
  clientAuth.sendOOB = ({ oob }) => {
    do_print(oob);
    // Pass to server, skipping prompt for tests
    oobData.resolve(oob);
  };

  let transport = yield DebuggerClient.socketConnect({
    host: "127.0.0.1",
    port: listener.port,
    encryption: true,
    authenticator: clientAuth,
    cert: {
      sha256: serverCert.sha256Fingerprint
    }
  });
  ok(transport, "Client transport created");

  let client = new DebuggerClient(transport);
  let onUnexpectedClose = () => {
    do_throw("Closed unexpectedly");
  };
  client.addListener("closed", onUnexpectedClose);
  yield connectClient(client);

  // Send a message the server will echo back
  let message = "secrets";
  let reply = yield client.request({
    to: "root",
    type: "echo",
    message
  });
  equal(reply.message, message, "Encrypted echo matches");

  client.removeListener("closed", onUnexpectedClose);
  transport.close();
  listener.close();
  equal(DebuggerServer.listeningSockets, 0, "0 listening sockets");
});

// Client w/o OOB_CERT auth fails to connect to server w/ OOB_CERT auth
add_task(function*() {
  equal(DebuggerServer.listeningSockets, 0, "0 listening sockets");

  let oobData = promise.defer();
  let AuthenticatorType = DebuggerServer.Authenticators.get("OOB_CERT");
  let serverAuth = new AuthenticatorType.Server();
  serverAuth.allowConnection = () => {
    return DebuggerServer.AuthenticationResult.ALLOW;
  };
  serverAuth.receiveOOB = () => oobData.promise; // Skip prompt for tests

  let listener = DebuggerServer.createListener();
  ok(listener, "Socket listener created");
  listener.portOrPath = -1 /* any available port */;
  listener.encryption = true /* required for OOB_CERT */;
  listener.authenticator = serverAuth;
  yield listener.open();
  equal(DebuggerServer.listeningSockets, 1, "1 listening socket");

  // This will succeed, but leaves the client in confused state, and no data is
  // actually accessible
  let transport = yield DebuggerClient.socketConnect({
    host: "127.0.0.1",
    port: listener.port,
    encryption: true
    // authenticator: PROMPT is the default
  });

  // Attempt to use the transport
  let deferred = promise.defer();
  let client = new DebuggerClient(transport);
  client.onPacket = packet => {
    // Client did not authenticate, so it ends up seeing the server's auth data
    // which is effectively malformed data from the client's perspective
    ok(!packet.from && packet.authResult, "Got auth packet instead of data");
    deferred.resolve();
  };
  client.connect();
  yield deferred.promise;

  // Try to send a message the server will echo back
  let message = "secrets";
  try {
    yield client.request({
      to: "root",
      type: "echo",
      message
    });
  } catch(e) {
    ok(true, "Sending a message failed");
    transport.close();
    listener.close();
    equal(DebuggerServer.listeningSockets, 0, "0 listening sockets");
    return;
  }

  do_throw("Connection unexpectedly succeeded");
});

// Client w/ invalid K value fails to connect
add_task(function*() {
  equal(DebuggerServer.listeningSockets, 0, "0 listening sockets");

  // Grab our cert, instead of relying on a discovery advertisement
  let serverCert = yield cert.local.getOrCreate();

  let oobData = promise.defer();
  let AuthenticatorType = DebuggerServer.Authenticators.get("OOB_CERT");
  let serverAuth = new AuthenticatorType.Server();
  serverAuth.allowConnection = () => {
    return DebuggerServer.AuthenticationResult.ALLOW;
  };
  serverAuth.receiveOOB = () => oobData.promise; // Skip prompt for tests

  let clientAuth = new AuthenticatorType.Client();
  clientAuth.sendOOB = ({ oob }) => {
    do_print(oob);
    do_print("Modifying K value, should fail");
    // Pass to server, skipping prompt for tests
    oobData.resolve({
      k: oob.k + 1,
      sha256: oob.sha256
    });
  };

  let listener = DebuggerServer.createListener();
  ok(listener, "Socket listener created");
  listener.portOrPath = -1 /* any available port */;
  listener.encryption = true /* required for OOB_CERT */;
  listener.authenticator = serverAuth;
  yield listener.open();
  equal(DebuggerServer.listeningSockets, 1, "1 listening socket");

  try {
    yield DebuggerClient.socketConnect({
      host: "127.0.0.1",
      port: listener.port,
      encryption: true,
      authenticator: clientAuth,
      cert: {
        sha256: serverCert.sha256Fingerprint
      }
    });
  } catch(e) {
    ok(true, "Client failed to connect as expected");
    listener.close();
    equal(DebuggerServer.listeningSockets, 0, "0 listening sockets");
    return;
  }

  do_throw("Connection unexpectedly succeeded");
});

// Client w/ invalid cert hash fails to connect
add_task(function*() {
  equal(DebuggerServer.listeningSockets, 0, "0 listening sockets");

  // Grab our cert, instead of relying on a discovery advertisement
  let serverCert = yield cert.local.getOrCreate();

  let oobData = promise.defer();
  let AuthenticatorType = DebuggerServer.Authenticators.get("OOB_CERT");
  let serverAuth = new AuthenticatorType.Server();
  serverAuth.allowConnection = () => {
    return DebuggerServer.AuthenticationResult.ALLOW;
  };
  serverAuth.receiveOOB = () => oobData.promise; // Skip prompt for tests

  let clientAuth = new AuthenticatorType.Client();
  clientAuth.sendOOB = ({ oob }) => {
    do_print(oob);
    do_print("Modifying cert hash, should fail");
    // Pass to server, skipping prompt for tests
    oobData.resolve({
      k: oob.k,
      sha256: oob.sha256 + 1
    });
  };

  let listener = DebuggerServer.createListener();
  ok(listener, "Socket listener created");
  listener.portOrPath = -1 /* any available port */;
  listener.encryption = true /* required for OOB_CERT */;
  listener.authenticator = serverAuth;
  yield listener.open();
  equal(DebuggerServer.listeningSockets, 1, "1 listening socket");

  try {
    yield DebuggerClient.socketConnect({
      host: "127.0.0.1",
      port: listener.port,
      encryption: true,
      authenticator: clientAuth,
      cert: {
        sha256: serverCert.sha256Fingerprint
      }
    });
  } catch(e) {
    ok(true, "Client failed to connect as expected");
    listener.close();
    equal(DebuggerServer.listeningSockets, 0, "0 listening sockets");
    return;
  }

  do_throw("Connection unexpectedly succeeded");
});

add_task(function*() {
  DebuggerServer.destroy();
});
