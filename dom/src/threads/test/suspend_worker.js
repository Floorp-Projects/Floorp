var counter = 0;

setInterval(function() {
  postMessage(++counter);
}, 100);
