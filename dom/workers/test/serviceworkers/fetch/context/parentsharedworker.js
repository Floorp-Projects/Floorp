onconnect = function(e) {
  e.ports[0].start();
  var worker = new Worker("myworker?shared");
  worker.onmessage = function(e2) {
    e.ports[0].postMessage(e2.data);
    self.close();
  };
};
