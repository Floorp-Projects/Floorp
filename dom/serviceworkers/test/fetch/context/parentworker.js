var worker = new Worker("myworker");
worker.onmessage = function(e) {
  postMessage(e.data);
};
