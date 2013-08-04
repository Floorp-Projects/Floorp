/**
 * Test TCPSocket.js by creating an XPCOM-style server socket, then sending
 * data in both directions and making sure each side receives their data
 * correctly and with the proper events.
 *
 * This test is derived from netwerk/test/unit/test_socks.js, except we don't
 * involve a subprocess.
 *
 * Future work:
 * - SSL.  see https://bugzilla.mozilla.org/show_bug.cgi?id=466524
 *             https://bugzilla.mozilla.org/show_bug.cgi?id=662180
 *   Alternatively, mochitests could be used.
 * - Testing overflow logic.
 *
 **/

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
const CC = Components.Constructor;

/**
 *
 * Constants
 *
 */

// Test parameter.
const PORT = 8085;
const BACKLOG = -1;

// Some binary data to send.
const DATA_ARRAY = [0, 255, 254, 0, 1, 2, 3, 0, 255, 255, 254, 0],
      DATA_ARRAY_BUFFER = new ArrayBuffer(DATA_ARRAY.length),
      TYPED_DATA_ARRAY = new Uint8Array(DATA_ARRAY_BUFFER),
      HELLO_WORLD = "hlo wrld. ",
      BIG_ARRAY = new Array(65539);

TYPED_DATA_ARRAY.set(DATA_ARRAY, 0);

for (var i_big = 0; i_big < BIG_ARRAY.length; i_big++) {
  BIG_ARRAY[i_big] = Math.floor(Math.random() * 256);
}

const BIG_ARRAY_BUFFER = new ArrayBuffer(BIG_ARRAY.length);
const BIG_TYPED_ARRAY = new Uint8Array(BIG_ARRAY_BUFFER);
BIG_TYPED_ARRAY.set(BIG_ARRAY);

const TCPSocket = new (CC("@mozilla.org/tcp-socket;1",
                     "nsIDOMTCPSocket"))();

const gInChild = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
                  .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

/**
 *
 * Helper functions
 *
 */


function makeSuccessCase(name) {
  return function() {
    do_print('got expected: ' + name);
    run_next_test();
  };
}

function makeJointSuccess(names) {
  let funcs = {}, successCount = 0;
  names.forEach(function(name) {
    funcs[name] = function() {
      do_print('got expected: ' + name);
      if (++successCount === names.length)
        run_next_test();
    };
  });
  return funcs;
}

function makeFailureCase(name) {
  return function() {
    let argstr;
    if (arguments.length) {
      argstr = '(args: ' +
        Array.map(arguments, function(x) { return x.data + ""; }).join(" ") + ')';
    }
    else {
      argstr = '(no arguments)';
    }
    do_throw('got unexpected: ' + name + ' ' + argstr);
  };
}

function makeExpectData(name, expectedData, fromEvent, callback) {
  let dataBuffer = fromEvent ? null : [], done = false;
  let dataBufferView = null;
  return function(receivedData) {
    if (receivedData.data) {
      receivedData = receivedData.data;
    }
    let recvLength = receivedData.byteLength !== undefined ?
        receivedData.byteLength : receivedData.length;

    if (fromEvent) {
      if (dataBuffer) {
        let newBuffer = new ArrayBuffer(dataBuffer.byteLength + recvLength);
        let newBufferView = new Uint8Array(newBuffer);
        newBufferView.set(dataBufferView, 0);
        newBufferView.set(receivedData, dataBuffer.byteLength);
        dataBuffer = newBuffer;
        dataBufferView = newBufferView;
      }
      else {
        dataBuffer = receivedData;
        dataBufferView = new Uint8Array(dataBuffer);
      }
    }
    else {
      dataBuffer = dataBuffer.concat(receivedData);
    }
    do_print(name + ' received ' + recvLength + ' bytes');

    if (done)
      do_throw(name + ' Received data event when already done!');

    let dataView = dataBuffer.byteLength !== undefined ? new Uint8Array(dataBuffer) : dataBuffer;
    if (dataView.length >= expectedData.length) {
      // check the bytes are equivalent
      for (let i = 0; i < expectedData.length; i++) {
        if (dataView[i] !== expectedData[i]) {
          do_throw(name + ' Received mismatched character at position ' + i);
        }
      }
      if (dataView.length > expectedData.length)
        do_throw(name + ' Received ' + dataView.length + ' bytes but only expected ' +
                 expectedData.length + ' bytes.');

      done = true;
      if (callback) {
        callback();
      } else {
        run_next_test();
      }
    }
  };
}

var server = null, sock = null, connectedsock = null, failure_drain = null;
var count = 0;
/**
 *
 * Test functions
 *
 */

/**
 * Connect the socket to the server. This test is added as the first
 * test, and is also added after every test which results in the socket
 * being closed.
 */

function connectSock() {
  if (server) {
    server.close();
  }

  var yayFuncs = makeJointSuccess(['serveropen', 'clientopen']);
  var options = { binaryType: 'arraybuffer' };

  server = TCPSocket.listen(PORT, options, BACKLOG);
  server.onconnect = function(socket) {
    connectedsock = socket;
    connectedsock.ondata = makeFailureCase('serverdata');
    connectedsock.onerror = makeFailureCase('servererror');
    connectedsock.onclose = makeFailureCase('serverclose');
    yayFuncs.serveropen();
  };
  server.onerror = makeFailureCase('error');
  sock = TCPSocket.open(
    '127.0.0.1', PORT, options);
  sock.onopen = yayFuncs.clientopen;
  sock.ondrain = null;
  sock.ondata = makeFailureCase('data');
  sock.onerror = makeFailureCase('error');
  sock.onclose = makeFailureCase('close');  
}

/**
 * Connect the socket to the server after the server was closed.
 * This test is added after test to close the server was conducted.
 */
function openSockInClosingServer() {
  var success = makeSuccessCase('clientnotopen');
  var options = { binaryType: 'arraybuffer' };
  
  sock = TCPSocket.open(
    '127.0.0.1', PORT, options);

  sock.onopen = makeFailureCase('open');
  sock.onerror = success();
}

/**
 * Test that sending a small amount of data works, and that buffering
 * does not take place for this small amount of data.
 */

function sendDataToServer() {
  connectedsock.ondata = makeExpectData('serverdata', DATA_ARRAY, true);
  if (!sock.send(DATA_ARRAY_BUFFER)) {
    do_throw("send should not have buffered such a small amount of data");
  }
}

/**
 * Test that data sent from the server correctly fires the ondata
 * callback on the client side.
 */

function receiveDataFromServer() {
  connectedsock.ondata = makeFailureCase('serverdata');
  sock.ondata = makeExpectData('data', DATA_ARRAY, true);

  connectedsock.send(DATA_ARRAY_BUFFER);
}

/**
 * Test that when the server closes the connection, the onclose callback
 * is fired on the client side.
 */

function serverCloses() {
  // we don't really care about the server's close event, but we do want to
  // make sure it happened for sequencing purposes.
  sock.ondata = makeFailureCase('data');
  sock.onclose = makeFailureCase('close1');
  connectedsock.onclose = makeFailureCase('close2');

  server.close();
  run_next_test();
}

/**
 * Test that when the client closes the connection, the onclose callback
 * is fired on the server side.
 */

function cleanup() {
  do_print("Cleaning up");
  sock.onclose = null;
  connectedsock.onclose = null;

  server.close();
  sock.close();
  if (count == 1){
    if (!gInChild)
      Services.prefs.clearUserPref('dom.mozTCPSocket.enabled');
  }
  count++;
  run_next_test();
}
// - connect, data and events work both ways
add_test(connectSock);

add_test(sendDataToServer);

add_test(receiveDataFromServer);
// - server closes on us
add_test(serverCloses);

// - send and receive after closing server
add_test(sendDataToServer);
add_test(receiveDataFromServer);
// - check a connection refused from client to server after closing server
add_test(serverCloses);

add_test(openSockInClosingServer);

// - clean up
add_test(cleanup);

// - send and receive in reverse order for client and server
add_test(connectSock);

add_test(receiveDataFromServer);

add_test(sendDataToServer);

// - clean up

add_test(cleanup);

function run_test() {
  if (!gInChild)
    Services.prefs.setBoolPref('dom.mozTCPSocket.enabled', true);

  run_next_test();

  do_timeout(10000, function() {
    if (server) {
      server.close();
    }

    do_throw(
      "The test should never take this long unless the system is hosed.");
  });
}
