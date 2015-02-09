onconnect = function(evt) {
  evt.ports[0].onmessage = function(evt) {
    var bc = new BroadcastChannel('foobar');
    bc.addEventListener('message', function(event) {
      bc.postMessage(event.data == "hello world from the window" ?
                       "hello world from the worker" : "KO");
      bc.close();
    }, false);

    evt.target.postMessage("READY");
  }
}
