self.addEventListener("connect", function(e) {
  var port = e.ports[0];
  port.onmessage = function(msg) {
    port.postMessage(eval(msg.data));
  };
});
