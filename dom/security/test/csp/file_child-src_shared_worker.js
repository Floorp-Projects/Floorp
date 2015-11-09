onconnect = function(e) {
  var port = e.ports[0];
  port.addEventListener('message', function(e) {
    port.postMessage('success');
  });

  port.start();
}
