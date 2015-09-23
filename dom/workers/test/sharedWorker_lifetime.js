onconnect = function(e) {
  setTimeout(function() {
    e.ports[0].postMessage("Still alive!");
  }, 500);
}
