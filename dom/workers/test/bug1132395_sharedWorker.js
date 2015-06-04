dump("SW created\n");
onconnect = function(evt) {
  dump("SW onconnect\n");
  evt.ports[0].onmessage = function(e) {
    dump("SW onmessage\n");
    var blob = new Blob(['123'], { type: 'text/plain' });
    dump("SW blob created\n");
    var url = URL.createObjectURL(blob);
    dump("SW url created: " + url + "\n");
    evt.ports[0].postMessage('alive \\o/');
  };
}
