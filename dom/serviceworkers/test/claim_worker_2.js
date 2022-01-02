onactivate = function(e) {
  var result = {
    resolve_value: false,
    match_count_before: -1,
    match_count_after: -1,
    message: "claim_worker_2",
  };

  self.clients
    .matchAll()
    .then(function(matched) {
      // should be 0
      result.match_count_before = matched.length;
    })
    .then(function() {
      return clients.claim();
    })
    .then(function(ret) {
      result.resolve_value = ret;
      return clients.matchAll();
    })
    .then(function(matched) {
      // should be 1
      result.match_count_after = matched.length;
      if (result.match_count_after === 1) {
        matched[0].postMessage(result);
      } else {
        dump("ERROR: claim_worker_2 failed to capture clients.\n");
        for (let i = 0; i < matched.length; ++i) {
          dump("### ### matched[" + i + "]: " + matched[i].url + "\n");
        }
      }
    });
};
