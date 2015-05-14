onactivate = function(e) {
  var result = {
    resolve_value: false,
    match_count_before: -1,
    match_count_after: -1,
    message: "claim_worker_1"
  };

  self.clients.matchAll().then(function(matched) {
    // should be 0
    result.match_count_before = matched.length;
  }).then(function() {
    var claimPromise = self.clients.claim().then(function(ret) {
      result.resolve_value = ret;
    });

    return claimPromise.then(self.clients.matchAll().then(function(matched) {
      // should be 2
      result.match_count_after = matched.length;
      for (i = 0; i < matched.length; i++) {
        matched[i].postMessage(result);
      }
      if (result.match_count_after !== 2) {
        dump("ERROR: claim_worker_1 failed to capture clients.\n");
      }
    }));
  });
}
