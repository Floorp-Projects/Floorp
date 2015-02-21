function loop() {
  self.clients.matchAll().then(function(result) {
    setTimeout(loop, 0);
  });
}

onactivate = function(e) {
  // spam matchAll until the worker is closed.
  loop();
}
