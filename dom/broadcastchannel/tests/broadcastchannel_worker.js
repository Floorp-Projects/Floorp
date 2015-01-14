onmessage = function(evt) {
  if (evt.data != 0) {
    var worker = new Worker("broadcastchannel_worker.js");
    worker.onmessage = function(evt) {
      postMessage(evt.data);
    }
    worker.postMessage(evt.data - 1);
    return;
  }

  var bc = new BroadcastChannel('foobar');
  bc.addEventListener('message', function(event) {
    bc.postMessage(event.data == "hello world from the window" ? "hello world from the worker" : "KO");
    bc.close();
  }, false);

  postMessage("READY");
}
