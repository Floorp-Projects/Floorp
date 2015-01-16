(new BroadcastChannel('foobar')).onmessage = function(event) {
  event.target.postMessage(event.data);
}

postMessage("READY");
