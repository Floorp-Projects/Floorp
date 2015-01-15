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
    postMessage(event.data == "hello world from the window" ? "OK" : "KO");
    bc.postMessage("hello world from the worker");
    bc.close();
  }, false);

  postMessage("READY");
}
