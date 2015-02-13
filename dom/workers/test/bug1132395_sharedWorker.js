onconnect = function(evt) {
  var blob = new Blob(['123'], { type: 'text/plain' });
  var url = URL.createObjectURL(blob);
  evt.ports[0].postMessage('alive \\o/');
}
