this.EXPORTED_SYMBOLS = ['createSocket', 'createServer', 'enablePrefsAndPermissions'];

this.createSocket = function(host, port, options) {
  return new TCPSocket(host, port, options);
}

this.createServer = function(port, options, backlog) {
  return new TCPServerSocket(port, options, backlog);
}

this.enablePrefsAndPermissions = function() {
  return false;
}
