this.EXPORTED_SYMBOLS = ['checkFromJSM'];

this.checkFromJSM = function checkFromJSM(ok, is, finish) {
  let worker = new ChromeWorker("jsm_url_worker.js");
  worker.onmessage = function(event) {

   if (event.data.type == 'finish') {
     finish();
    } else if (event.data.type == 'status') {
      ok(event.data.status, event.data.msg);
    }
  }

  var self = this;
  worker.onerror = function(event) {
    is(event.target, worker);
    ok(false, "Worker had an error: " + event.data);
    self.worker.terminate();
    finish();
  };

  worker.postMessage(0);
}
