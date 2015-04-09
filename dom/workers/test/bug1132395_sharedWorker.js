onconnect = function(evt) {
  evt.ports[0].onmessage = function(e) {
    var blob = new Blob(['123'], { type: 'text/plain' });
    var url = URL.createObjectURL(blob);
    evt.ports[0].postMessage('alive \\o/');
  };
}
