var counter = 0;
onconnect = function(evt) {
  evt.ports[0].postMessage(++counter);
}

