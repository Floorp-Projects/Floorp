self.addEventListener("connect", function (e) {
  var port = e.ports[0];
  port.onmessage = function (msg) {
    // eslint-disable-next-line no-eval
    port.postMessage(eval(msg.data));
  };
});
