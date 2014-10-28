function loop() {
  self.clients.getServiced().then(function(result) {
    setTimeout(loop, 0);
  });
}

onactivate = function(e) {
  // spam getServiced until the worker is closed.
  loop();
}
