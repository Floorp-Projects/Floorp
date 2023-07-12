"use strict";

// These are defined in test_tcpsocket_client_and_server_basics.html
/* global createServer, createSocket, socketCompartmentInstanceOfArrayBuffer */

// Bug 788960 and later bug 1329245 have taught us that attempting to connect to
// a port that is not listening or is no longer listening fails to consistently
// result in the error (or any) event we expect on Darwin/OSX/"OS X".
const isOSX = Services.appinfo.OS === "Darwin";
const testConnectingToNonListeningPort = !isOSX;

const SERVER_BACKLOG = -1;

const SOCKET_EVENTS = ["open", "data", "drain", "error", "close"];

function concatUint8Arrays(a, b) {
  let newArr = new Uint8Array(a.length + b.length);
  newArr.set(a, 0);
  newArr.set(b, a.length);
  return newArr;
}

function assertUint8ArraysEqual(a, b, comparingWhat) {
  if (a.length !== b.length) {
    ok(
      false,
      comparingWhat +
        " arrays do not have the same length; " +
        a.length +
        " versus " +
        b.length
    );
    return;
  }
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) {
      ok(
        false,
        comparingWhat +
          " arrays differ at index " +
          i +
          a[i] +
          " versus " +
          b[i]
      );
      return;
    }
  }
  ok(true, comparingWhat + " arrays were equivalent.");
}

/**
 * Helper method to add event listeners to a socket and provide two Promise-returning
 * helpers (see below for docs on them).  This *must* be called during the turn of
 * the event loop where TCPSocket's constructor is called or the onconnect method is being
 * invoked.
 */
function listenForEventsOnSocket(socket, socketType) {
  let wantDataLength = null;
  let wantDataAndClose = false;
  let pendingResolve = null;
  let receivedEvents = [];
  let receivedData = null;
  let handleGenericEvent = function (event) {
    dump("(" + socketType + " event: " + event.type + ")\n");
    if (pendingResolve && wantDataLength === null) {
      pendingResolve(event);
      pendingResolve = null;
    } else {
      receivedEvents.push(event);
    }
  };

  socket.onopen = handleGenericEvent;
  socket.ondrain = handleGenericEvent;
  socket.onerror = handleGenericEvent;
  socket.onclose = function (event) {
    if (!wantDataAndClose) {
      handleGenericEvent(event);
    } else if (pendingResolve) {
      dump("(" + socketType + " event: close)\n");
      pendingResolve(receivedData);
      pendingResolve = null;
      wantDataAndClose = false;
    }
  };
  socket.ondata = function (event) {
    dump(
      "(" +
        socketType +
        " event: " +
        event.type +
        " length: " +
        event.data.byteLength +
        ")\n"
    );
    ok(
      socketCompartmentInstanceOfArrayBuffer(event.data),
      "payload is ArrayBuffer"
    );
    var arr = new Uint8Array(event.data);
    if (receivedData === null) {
      receivedData = arr;
    } else {
      receivedData = concatUint8Arrays(receivedData, arr);
    }
    if (wantDataLength !== null && receivedData.length >= wantDataLength) {
      pendingResolve(receivedData);
      pendingResolve = null;
      receivedData = null;
      wantDataLength = null;
    }
  };

  return {
    /**
     * Return a Promise that will be resolved with the next (non-data) event
     * received by the socket.  If there are queued events, the Promise will
     * be immediately resolved (but you won't see that until a future turn of
     * the event loop).
     */
    waitForEvent() {
      if (pendingResolve) {
        throw new Error("only one wait allowed at a time.");
      }

      if (receivedEvents.length) {
        return Promise.resolve(receivedEvents.shift());
      }

      dump("(" + socketType + " waiting for event)\n");
      return new Promise(function (resolve, reject) {
        pendingResolve = resolve;
      });
    },
    /**
     * Return a Promise that will be resolved with a Uint8Array of at least the
     * given length.  We buffer / accumulate received data until we have enough
     * data.  Data is buffered even before you call this method, so be sure to
     * explicitly wait for any and all data sent by the other side.
     */
    waitForDataWithAtLeastLength(length) {
      if (pendingResolve) {
        throw new Error("only one wait allowed at a time.");
      }
      if (receivedData && receivedData.length >= length) {
        let promise = Promise.resolve(receivedData);
        receivedData = null;
        return promise;
      }
      dump("(" + socketType + " waiting for " + length + " bytes)\n");
      return new Promise(function (resolve, reject) {
        pendingResolve = resolve;
        wantDataLength = length;
      });
    },
    waitForAnyDataAndClose() {
      if (pendingResolve) {
        throw new Error("only one wait allowed at a time.");
      }

      return new Promise(function (resolve, reject) {
        pendingResolve = resolve;
        // we may receive no data before getting close, in which case we want to
        // return an empty array
        receivedData = new Uint8Array();
        wantDataAndClose = true;
      });
    },
  };
}

/**
 * Return a promise that is resolved when the server receives a connection.  The
 * promise is resolved with { socket, queue } where `queue` is the result of
 * calling listenForEventsOnSocket(socket).  This must be done because we need
 * to add the event listener during the connection.
 */
function waitForConnection(listeningServer) {
  return new Promise(function (resolve, reject) {
    // Because of the event model of sockets, we can't use the
    // listenForEventsOnSocket mechanism; we need to hook up listeners during
    // the connect event.
    listeningServer.onconnect = function (event) {
      // Clobber the listener to get upset if it receives any more connections
      // after this.
      listeningServer.onconnect = function () {
        ok(false, "Received a connection when not expecting one.");
      };
      ok(true, "Listening server accepted socket");
      resolve({
        socket: event.socket,
        queue: listenForEventsOnSocket(event.socket, "server"),
      });
    };
  });
}

function defer() {
  var deferred = {};
  deferred.promise = new Promise(function (resolve, reject) {
    deferred.resolve = resolve;
    deferred.reject = reject;
  });
  return deferred;
}

async function test_basics() {
  // See bug 903830; in e10s mode we never get to find out the localPort if we
  // let it pick a free port by choosing 0.  This is the same port the xpcshell
  // test was using.
  let serverPort = 8085;

  // - Start up a listening socket.
  let listeningServer = createServer(
    serverPort,
    { binaryType: "arraybuffer" },
    SERVER_BACKLOG
  );

  let connectedPromise = waitForConnection(listeningServer);

  // -- Open a connection to the server
  let clientSocket = createSocket("127.0.0.1", serverPort, {
    binaryType: "arraybuffer",
  });
  let clientQueue = listenForEventsOnSocket(clientSocket, "client");

  // (the client connects)
  is((await clientQueue.waitForEvent()).type, "open", "got open event");
  is(clientSocket.readyState, "open", "client readyState is open");

  // (the server connected)
  let { socket: serverSocket, queue: serverQueue } = await connectedPromise;
  is(serverSocket.readyState, "open", "server readyState is open");

  // -- Simple send / receive
  // - Send data from client to server
  // (But not so much we cross the drain threshold.)
  let smallUint8Array = new Uint8Array(256);
  for (let i = 0; i < smallUint8Array.length; i++) {
    smallUint8Array[i] = i;
  }
  is(
    clientSocket.send(smallUint8Array.buffer, 0, smallUint8Array.length),
    true,
    "Client sending less than 64k, buffer should not be full."
  );

  let serverReceived = await serverQueue.waitForDataWithAtLeastLength(256);
  assertUint8ArraysEqual(
    serverReceived,
    smallUint8Array,
    "Server received/client sent"
  );

  // - Send data from server to client
  // (But not so much we cross the drain threshold.)
  is(
    serverSocket.send(smallUint8Array.buffer, 0, smallUint8Array.length),
    true,
    "Server sending less than 64k, buffer should not be full."
  );

  let clientReceived = await clientQueue.waitForDataWithAtLeastLength(256);
  assertUint8ArraysEqual(
    clientReceived,
    smallUint8Array,
    "Client received/server sent"
  );

  // -- Perform sending multiple times with different buffer slices
  // - Send data from client to server
  // (But not so much we cross the drain threshold.)
  is(
    clientSocket.send(smallUint8Array.buffer, 0, 7),
    true,
    "Client sending less than 64k, buffer should not be full."
  );
  is(
    clientSocket.send(smallUint8Array.buffer, 7, smallUint8Array.length - 7),
    true,
    "Client sending less than 64k, buffer should not be full."
  );

  serverReceived = await serverQueue.waitForDataWithAtLeastLength(256);
  assertUint8ArraysEqual(
    serverReceived,
    smallUint8Array,
    "Server received/client sent"
  );

  // - Send data from server to client
  // (But not so much we cross the drain threshold.)
  is(
    serverSocket.send(smallUint8Array.buffer, 0, 7),
    true,
    "Server sending less than 64k, buffer should not be full."
  );
  is(
    serverSocket.send(smallUint8Array.buffer, 7, smallUint8Array.length - 7),
    true,
    "Server sending less than 64k, buffer should not be full."
  );

  clientReceived = await clientQueue.waitForDataWithAtLeastLength(256);
  assertUint8ArraysEqual(
    clientReceived,
    smallUint8Array,
    "Client received/server sent"
  );

  // -- Send "big" data in both directions
  // (Enough to cross the buffering/drain threshold; 64KiB)
  let bigUint8Array = new Uint8Array(65536 + 3);
  for (let i = 0; i < bigUint8Array.length; i++) {
    bigUint8Array[i] = i % 256;
  }
  // This can be anything from 1 to 65536. The idea is spliting and sending
  // bigUint8Array in two chunks should trigger ondrain the same as sending
  // bigUint8Array in one chunk.
  let lengthOfChunk1 = 65536;
  is(
    clientSocket.send(bigUint8Array.buffer, 0, lengthOfChunk1),
    true,
    "Client sending chunk1 should not result in the buffer being full."
  );
  // Do this twice so we have confidence that the 'drain' event machinery
  // doesn't break after the first use. The first time we send bigUint8Array in
  // two chunks, the second time we send bigUint8Array in one chunk.
  for (let iSend = 0; iSend < 2; iSend++) {
    // - Send "big" data from the client to the server
    let offset = iSend == 0 ? lengthOfChunk1 : 0;
    is(
      clientSocket.send(bigUint8Array.buffer, offset, bigUint8Array.length),
      false,
      "Client sending more than 64k should result in the buffer being full."
    );
    is(
      (await clientQueue.waitForEvent()).type,
      "drain",
      "The drain event should fire after a large send that indicated full."
    );

    serverReceived = await serverQueue.waitForDataWithAtLeastLength(
      bigUint8Array.length
    );
    assertUint8ArraysEqual(
      serverReceived,
      bigUint8Array,
      "server received/client sent"
    );

    if (iSend == 0) {
      is(
        serverSocket.send(bigUint8Array.buffer, 0, lengthOfChunk1),
        true,
        "Server sending chunk1 should not result in the buffer being full."
      );
    }
    // - Send "big" data from the server to the client
    is(
      serverSocket.send(bigUint8Array.buffer, offset, bigUint8Array.length),
      false,
      "Server sending more than 64k should result in the buffer being full."
    );
    is(
      (await serverQueue.waitForEvent()).type,
      "drain",
      "The drain event should fire after a large send that indicated full."
    );

    clientReceived = await clientQueue.waitForDataWithAtLeastLength(
      bigUint8Array.length
    );
    assertUint8ArraysEqual(
      clientReceived,
      bigUint8Array,
      "client received/server sent"
    );
  }

  // -- Server closes the connection
  serverSocket.close();
  is(
    serverSocket.readyState,
    "closing",
    "readyState should be closing immediately after calling close"
  );

  is(
    (await clientQueue.waitForEvent()).type,
    "close",
    "The client should get a close event when the server closes."
  );
  is(
    clientSocket.readyState,
    "closed",
    "client readyState should be closed after close event"
  );
  is(
    (await serverQueue.waitForEvent()).type,
    "close",
    "The server should get a close event when it closes itself."
  );
  is(
    serverSocket.readyState,
    "closed",
    "server readyState should be closed after close event"
  );

  // -- Re-establish connection
  connectedPromise = waitForConnection(listeningServer);
  clientSocket = createSocket("127.0.0.1", serverPort, {
    binaryType: "arraybuffer",
  });
  clientQueue = listenForEventsOnSocket(clientSocket, "client");
  is((await clientQueue.waitForEvent()).type, "open", "got open event");

  let connectedResult = await connectedPromise;
  // destructuring assignment is not yet ES6 compliant, must manually unpack
  serverSocket = connectedResult.socket;
  serverQueue = connectedResult.queue;

  // -- Client closes the connection
  clientSocket.close();
  is(
    clientSocket.readyState,
    "closing",
    "client readyState should be losing immediately after calling close"
  );

  is(
    (await clientQueue.waitForEvent()).type,
    "close",
    "The client should get a close event when it closes itself."
  );
  is(
    clientSocket.readyState,
    "closed",
    "client readyState should be closed after the close event is received"
  );
  is(
    (await serverQueue.waitForEvent()).type,
    "close",
    "The server should get a close event when the client closes."
  );
  is(
    serverSocket.readyState,
    "closed",
    "server readyState should be closed after the close event is received"
  );

  // -- Re-establish connection
  connectedPromise = waitForConnection(listeningServer);
  clientSocket = createSocket("127.0.0.1", serverPort, {
    binaryType: "arraybuffer",
  });
  clientQueue = listenForEventsOnSocket(clientSocket, "client");
  is((await clientQueue.waitForEvent()).type, "open", "got open event");

  connectedResult = await connectedPromise;
  // destructuring assignment is not yet ES6 compliant, must manually unpack
  serverSocket = connectedResult.socket;
  serverQueue = connectedResult.queue;

  // -- Call close after enqueueing a lot of data, make sure it goes through.
  // We'll have the client send and close.
  is(
    clientSocket.send(bigUint8Array.buffer, 0, bigUint8Array.length),
    false,
    "Client sending more than 64k should result in the buffer being full."
  );
  clientSocket.close();
  // The drain will still fire
  is(
    (await clientQueue.waitForEvent()).type,
    "drain",
    "The drain event should fire after a large send that returned true."
  );
  // Then we'll get a close
  is(
    (await clientQueue.waitForEvent()).type,
    "close",
    "The close event should fire after the drain event."
  );

  // The server will get its data
  serverReceived = await serverQueue.waitForDataWithAtLeastLength(
    bigUint8Array.length
  );
  assertUint8ArraysEqual(
    serverReceived,
    bigUint8Array,
    "server received/client sent"
  );
  // And a close.
  is(
    (await serverQueue.waitForEvent()).type,
    "close",
    "The drain event should fire after a large send that returned true."
  );

  // -- Re-establish connection
  connectedPromise = waitForConnection(listeningServer);
  clientSocket = createSocket("127.0.0.1", serverPort, {
    binaryType: "string",
  });
  clientQueue = listenForEventsOnSocket(clientSocket, "client");
  is((await clientQueue.waitForEvent()).type, "open", "got open event");

  connectedResult = await connectedPromise;
  // destructuring assignment is not yet ES6 compliant, must manually unpack
  serverSocket = connectedResult.socket;
  serverQueue = connectedResult.queue;

  // -- Attempt to send non-string data.
  // Restore the original behavior by replacing toString with
  // Object.prototype.toString. (bug 1121938)
  bigUint8Array.toString = Object.prototype.toString;
  is(
    clientSocket.send(bigUint8Array),
    true,
    "Client sending a large non-string should only send a small string."
  );
  clientSocket.close();
  // The server will get its data
  serverReceived = await serverQueue.waitForDataWithAtLeastLength(
    bigUint8Array.toString().length
  );
  // Then we'll get a close
  is(
    (await clientQueue.waitForEvent()).type,
    "close",
    "The close event should fire after the drain event."
  );

  // -- Re-establish connection (Test for Close Immediately)
  connectedPromise = waitForConnection(listeningServer);
  clientSocket = createSocket("127.0.0.1", serverPort, {
    binaryType: "arraybuffer",
  });
  clientQueue = listenForEventsOnSocket(clientSocket, "client");
  is((await clientQueue.waitForEvent()).type, "open", "got open event");

  connectedResult = await connectedPromise;
  // destructuring assignment is not yet ES6 compliant, must manually unpack
  serverSocket = connectedResult.socket;
  serverQueue = connectedResult.queue;

  // -- Attempt to send two non-string data.
  is(
    clientSocket.send(bigUint8Array.buffer, 0, bigUint8Array.length),
    false,
    "Server sending more than 64k should result in the buffer being full."
  );
  is(
    clientSocket.send(bigUint8Array.buffer, 0, bigUint8Array.length),
    false,
    "Server sending more than 64k should result in the buffer being full."
  );
  clientSocket.closeImmediately();

  serverReceived = await serverQueue.waitForAnyDataAndClose();

  is(
    serverReceived.length < 2 * bigUint8Array.length,
    true,
    "Received array length less than sent array length"
  );

  // -- Close the listening server (and try to connect)
  // We want to verify that the server actually closes / stops listening when
  // we tell it to.
  listeningServer.close();

  // (We don't run this check on OS X where it's flakey; see definition up top.)
  if (testConnectingToNonListeningPort) {
    // - try and connect, get an error
    clientSocket = createSocket("127.0.0.1", serverPort, {
      binaryType: "arraybuffer",
    });
    clientQueue = listenForEventsOnSocket(clientSocket, "client");
    is((await clientQueue.waitForEvent()).type, "error", "fail to connect");
    is(
      clientSocket.readyState,
      "closed",
      "client readyState should be closed after the failure to connect"
    );
  }
}

add_task(test_basics);

/**
 * Test that TCPSocket works with ipv6 address.
 */
add_task(async function test_ipv6() {
  const { HttpServer } = ChromeUtils.importESModule(
    "resource://testing-common/httpd.sys.mjs"
  );
  let deferred = defer();
  let httpServer = new HttpServer();
  httpServer.start_ipv6(-1);

  let clientSocket = new TCPSocket("::1", httpServer.identity.primaryPort);
  clientSocket.onopen = () => {
    ok(true, "Connect to ipv6 address succeeded");
    deferred.resolve();
  };
  clientSocket.onerror = () => {
    ok(false, "Connect to ipv6 address failed");
    deferred.reject();
  };
  await deferred.promise;
  await httpServer.stop();
});
