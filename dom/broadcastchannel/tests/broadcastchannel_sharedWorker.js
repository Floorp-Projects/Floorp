onconnect = function(evt) {
  evt.ports[0].onmessage = function(evt) {
    var bc = new BroadcastChannel('foobar');
    bc.addEventListener('message', function(event) {
      evt.target.postMessage(event.data == "hello world from the window" ? "OK" : "KO");
      bc.postMessage("hello world from the worker");
      bc.close();
    }, false);

    evt.target.postMessage("READY");
  }
}
