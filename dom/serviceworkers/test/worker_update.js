// For now this test only calls update to verify that our registration
// job queueing works properly when called from the worker thread. We should
// test actual update scenarios with a SJS test.
onmessage = function(e) {
  self.registration.update().then(function(v) {
    return v instanceof ServiceWorkerRegistration ? 'FINISH' : 'FAIL';
  }).catch(function(e) {
    return 'FAIL';
  }).then(function(result) {
    clients.matchAll().then(function(c) {
      if (c.length == 0) {
        dump("!!!!!!!!!!! WORKER HAS NO CLIENTS TO FINISH TEST !!!!!!!!!!!!\n");
        return;
      }

      c[0].postMessage(result);
    });
  });
}
