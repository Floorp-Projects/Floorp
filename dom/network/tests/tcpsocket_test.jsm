var EXPORTED_SYMBOLS = [
  "createSocket",
  "createServer",
  "socketCompartmentInstanceOfArrayBuffer",
];

var createSocket = function(host, port, options) {
  return new TCPSocket(host, port, options);
};

var createServer = function(port, options, backlog) {
  return new TCPServerSocket(port, options, backlog);
};

// See test_tcpsocket_client_and_server_basics.html's version for rationale.
var socketCompartmentInstanceOfArrayBuffer = function(obj) {
  return obj instanceof ArrayBuffer;
};
