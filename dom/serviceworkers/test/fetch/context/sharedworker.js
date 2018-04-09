onconnect = function(e) {
  e.ports[0].start();
  e.ports[0].postMessage("ack");
  self.close();
};
