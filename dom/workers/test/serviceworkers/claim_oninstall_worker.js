oninstall = function(e) {
  var claimFailedPromise = new Promise(function(resolve, reject) {
    clients.claim().then(reject, () => resolve());
  });

  e.waitUntil(claimFailedPromise);
}
