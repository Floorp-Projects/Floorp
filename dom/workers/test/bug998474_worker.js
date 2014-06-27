self.addEventListener("connect", function(e) {
  var port = e.ports[0];
  port.onmessage = function(e) {
    port.postMessage(eval(e.data));
  };
});
