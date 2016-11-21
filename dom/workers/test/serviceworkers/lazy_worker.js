onactivate = function(event) {
  var promise = new Promise(function(res) {
    setTimeout(function() {
      res();
      }, 500);
  });
  event.waitUntil(promise);
}
