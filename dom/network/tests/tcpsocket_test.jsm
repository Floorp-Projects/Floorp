this.EXPORTED_SYMBOLS = [
  'createSocket', 'createServer',
  'socketCompartmentInstanceOfArrayBuffer'];

this.createSocket = function(host, port, options) {
  return new TCPSocket(host, port, options);
}

this.createServer = function(port, options, backlog) {
  return new TCPServerSocket(port, options, backlog);
}

// See test_tcpsocket_client_and_server_basics.html's version for rationale.
this.socketCompartmentInstanceOfArrayBuffer = function(obj) {
  return obj instanceof ArrayBuffer;
}
