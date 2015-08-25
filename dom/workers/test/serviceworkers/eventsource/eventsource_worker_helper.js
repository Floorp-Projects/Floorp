function ok(aCondition, aMessage) {
  return new Promise(function(resolve, reject) {
    self.clients.matchAll().then(function(res) {
      if (!res.length) {
        reject();
        return;
      }
      res[0].postMessage({status: "callback", data: "ok", condition: aCondition, message: aMessage});
      resolve();
    });
  });
}
