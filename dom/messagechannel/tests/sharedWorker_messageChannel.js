onconnect = function(evt) {
  var mc = new MessageChannel();

  evt.ports[0].postMessage(42, [mc.port2]);
  mc.port1.onmessage = function(e) {
    mc.port1.postMessage(e.data);
  }
}
