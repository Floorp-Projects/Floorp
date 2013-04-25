const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;
const CC = Components.Constructor;

Cu.import("resource://gre/modules/Services.jsm");

const ServerSocket = CC("@mozilla.org/network/server-socket;1",
                        "nsIServerSocket",
                        "init"),
      InputStreamPump = CC("@mozilla.org/network/input-stream-pump;1",
                           "nsIInputStreamPump",
                           "init"),
      BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream"),
      BinaryOutputStream = CC("@mozilla.org/binaryoutputstream;1",
                              "nsIBinaryOutputStream",
                              "setOutputStream"),
      TCPSocket = new (CC("@mozilla.org/tcp-socket;1",
                     "nsIDOMTCPSocket"))();

var server = null, sock = null;

/**
 * Spin up a listening socket and associate at most one live, accepted socket
 * with ourselves.
 */
function TestServer() {
  this.listener = ServerSocket(-1, true, -1);
  do_print('server: listening on', this.listener.port);
  this.listener.asyncListen(this);

  this.binaryInput = null;
  this.input = null;
  this.binaryOutput = null;
  this.output = null;

  this.onaccept = null;
  this.ondata = null;
  this.onclose = null;
}

TestServer.prototype = {
  onSocketAccepted: function(socket, trans) {
    if (this.input)
      do_throw("More than one live connection!?");

    do_print('server: got client connection');
    this.input = trans.openInputStream(0, 0, 0);
    this.binaryInput = new BinaryInputStream(this.input);
    this.output = trans.openOutputStream(0, 0, 0);
    this.binaryOutput = new BinaryOutputStream(this.output);

    new InputStreamPump(this.input, -1, -1, 0, 0, false).asyncRead(this, null);

    if (this.onaccept)
      this.onaccept();
    else
      do_throw("Received unexpected connection!");
  },

  onStopListening: function(socket) {
  },

  onDataAvailable: function(request, context, inputStream, offset, count) {
    var readData = this.binaryInput.readByteArray(count);
    if (this.ondata) {
      try {
        this.ondata(readData);
      } catch(ex) {
        // re-throw if this is from do_throw
        if (ex === Cr.NS_ERROR_ABORT)
          throw ex;
        // log if there was a test problem
        do_print('Caught exception: ' + ex + '\n' + ex.stack);
        do_throw('test is broken; bad ondata handler; see above');
      }
    } else {
      do_throw('Received ' + count + ' bytes of unexpected data!');
    }
  },

  onStartRequest: function(request, context) {
  },

  onStopRequest: function(request, context, status) {
    if (this.onclose)
      this.onclose();
    else
      do_throw("Received unexpected close!");
  },

  close: function() {
    this.binaryInput.close();
    this.binaryOutput.close();
  },

  /**
   * Forget about the socket we knew about before.
   */
  reset: function() {
    this.binaryInput = null;
    this.input = null;
    this.binaryOutput = null;
    this.output = null;
  },
};

function run_test() {
  Services.prefs.setBoolPref('dom.mozTCPSocket.enabled', true);

  do_test_pending();

  server = new TestServer();
  server.reset();
  sock = TCPSocket.open(
    '127.0.0.1', server.listener.port,
    { binaryType: 'arraybuffer' });

  var encoder = new TextEncoder();
  var ok = encoder.encode("OKBYE");

  var expected = ['O', 'K', 'B', 'Y', 'E']
                   .map(function(c) { return c.charCodeAt(0); });
  var seenData = [];

  server.onaccept = function() {};
  server.ondata = function(data) {
    do_print(data + ":" + data.length);

    seenData = seenData.concat(data);

    if (seenData.length == expected.length) {
      do_print(expected);
      do_check_eq(seenData.length, expected.length);
      for (var i = 0; i < seenData.length; i++) {
        do_check_eq(seenData[i], expected[i]);
      }
      sock.close();
      server.close();
      do_test_finished();
    }
  };
  server.onclose = function() {};

  sock.onopen = function() {
    sock.send(ok.buffer, 0, 2);
    sock.send(ok.buffer, 2, 3);
  };
}