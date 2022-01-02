/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const cert = require("devtools/shared/security/cert");

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
  return client.connect();
}

add_task(async function() {
  initTestDevToolsServer();
});

// Client w/ OOB_CERT auth connects successfully to server w/ OOB_CERT auth
add_task(async function() {
  equal(DevToolsServer.listeningSockets, 0, "0 listening sockets");

  // Grab our cert, instead of relying on a discovery advertisement
  const serverCert = await cert.local.getOrCreate();

  const AuthenticatorType = DevToolsServer.Authenticators.get("OOB_CERT");
  const serverAuth = new AuthenticatorType.Server();
  serverAuth.allowConnection = () => {
    return DevToolsServer.AuthenticationResult.ALLOW;
  };
  // Skip prompt for tests
  const clientAuth = new AuthenticatorType.Client();
  serverAuth.receiveOOB = () =>
    new Promise(resolve => {
      clientAuth.sendOOB = ({ oob }) => {
        info(oob);
        // Pass to server, skipping prompt for tests
        resolve(oob);
      };
    });

  const socketOptions = {
    authenticator: serverAuth,
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
    authenticator: clientAuth,
    cert: {
      sha256: serverCert.sha256Fingerprint,
    },
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

// Client w/o OOB_CERT auth fails to connect to server w/ OOB_CERT auth
add_task(async function() {
  equal(DevToolsServer.listeningSockets, 0, "0 listening sockets");

  const AuthenticatorType = DevToolsServer.Authenticators.get("OOB_CERT");
  const serverAuth = new AuthenticatorType.Server();
  serverAuth.allowConnection = () => {
    return DevToolsServer.AuthenticationResult.ALLOW;
  };
  // Skip prompt for tests
  const clientAuth = new AuthenticatorType.Client();
  serverAuth.receiveOOB = () =>
    new Promise(resolve => {
      clientAuth.sendOOB = ({ oob }) => {
        info(oob);
        // Pass to server, skipping prompt for tests
        resolve(oob);
      };
    });

  const socketOptions = {
    authenticator: serverAuth,
    encryption: true,
    portOrPath: -1,
  };
  const listener = new SocketListener(DevToolsServer, socketOptions);
  ok(listener, "Socket listener created");
  await listener.open();
  equal(DevToolsServer.listeningSockets, 1, "1 listening socket");

  // This will succeed, but leaves the client in confused state, and no data is
  // actually accessible
  const transport = await DevToolsClient.socketConnect({
    host: "127.0.0.1",
    port: listener.port,
    encryption: true,
    // authenticator: PROMPT is the default
  });

  // Attempt to use the transport
  const client = new DevToolsClient(transport);
  const promise = new Promise(resolve => {
    client.onPacket = packet => {
      // Client did not authenticate, so it ends up seeing the server's auth data
      // which is effectively malformed data from the client's perspective
      ok(!packet.from && packet.authResult, "Got auth packet instead of data");
      resolve();
    };
  });
  client.connect();
  await promise;

  // Try to send a message the server will echo back
  const message = "secrets";
  try {
    await client.request({
      to: "root",
      type: "echo",
      message,
    });
  } catch (e) {
    ok(true, "Sending a message failed");
    transport.close();
    listener.close();
    equal(DevToolsServer.listeningSockets, 0, "0 listening sockets");
    return;
  }

  do_throw("Connection unexpectedly succeeded");
});

// Client w/ invalid K value fails to connect
add_task(async function() {
  equal(DevToolsServer.listeningSockets, 0, "0 listening sockets");

  // Grab our cert, instead of relying on a discovery advertisement
  const serverCert = await cert.local.getOrCreate();

  const AuthenticatorType = DevToolsServer.Authenticators.get("OOB_CERT");
  const serverAuth = new AuthenticatorType.Server();
  serverAuth.allowConnection = () => {
    return DevToolsServer.AuthenticationResult.ALLOW;
  };
  // Skip prompt for tests
  const clientAuth = new AuthenticatorType.Client();
  serverAuth.receiveOOB = () =>
    new Promise(resolve => {
      clientAuth.sendOOB = ({ oob }) => {
        info(oob);
        info("Modifying K value, should fail");
        // Pass to server, skipping prompt for tests
        resolve({
          k: oob.k + 1,
          sha256: oob.sha256,
        });
      };
    });

  const socketOptions = {
    authenticator: serverAuth,
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
      encryption: true,
      authenticator: clientAuth,
      cert: {
        sha256: serverCert.sha256Fingerprint,
      },
    });
  } catch (e) {
    ok(true, "Client failed to connect as expected");
    listener.close();
    equal(DevToolsServer.listeningSockets, 0, "0 listening sockets");
    return;
  }

  do_throw("Connection unexpectedly succeeded");
});

// Client w/ invalid cert hash fails to connect
add_task(async function() {
  equal(DevToolsServer.listeningSockets, 0, "0 listening sockets");

  // Grab our cert, instead of relying on a discovery advertisement
  const serverCert = await cert.local.getOrCreate();

  const AuthenticatorType = DevToolsServer.Authenticators.get("OOB_CERT");
  const serverAuth = new AuthenticatorType.Server();
  serverAuth.allowConnection = () => {
    return DevToolsServer.AuthenticationResult.ALLOW;
  };
  // Skip prompt for tests
  const clientAuth = new AuthenticatorType.Client();
  serverAuth.receiveOOB = () =>
    new Promise(resolve => {
      clientAuth.sendOOB = ({ oob }) => {
        info(oob);
        info("Modifying cert hash, should fail");
        // Pass to server, skipping prompt for tests
        resolve({
          k: oob.k,
          sha256: oob.sha256 + 1,
        });
      };
    });

  const socketOptions = {
    authenticator: serverAuth,
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
      encryption: true,
      authenticator: clientAuth,
      cert: {
        sha256: serverCert.sha256Fingerprint,
      },
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
