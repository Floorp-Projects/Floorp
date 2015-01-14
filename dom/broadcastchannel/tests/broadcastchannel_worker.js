onmessage = function(evt) {
  var bc = new BroadcastChannel('foobar');
  bc.addEventListener('message', function(event) {
    postMessage(event.data == "hello world from the window" ? "OK" : "KO");
    bc.postMessage("hello world from the worker");
  }, false);

  postMessage("READY");
}
