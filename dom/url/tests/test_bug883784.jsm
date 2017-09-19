this.EXPORTED_SYMBOLS = ["Test"];

Cu.importGlobalProperties(["URL", "XMLHttpRequest"]);

this.Test = {
  start: function(ok, is, finish) {
    let worker = new ChromeWorker("jsm_url_worker.js");
    worker.onmessage = function(event) {
      if (event.data.type == 'status') {
        ok(event.data.status, event.data.msg);
      } else if (event.data.type == 'url') {
        var xhr = new XMLHttpRequest();
        xhr.open('GET', event.data.url, false);
        xhr.onreadystatechange = function() {
          if (xhr.readyState == 4) {
            ok(true, "Blob readable!");
            URL.revokeObjectURL(event.data.url);
            finish();
          }
        }
        xhr.onerror = function() {
          ok(false, "Blob unreadable, should not happen!");
          URL.revokeObjectURL(event.data.url);
          finish();
        }
        xhr.send();
      }
    };

    var self = this;
    worker.onerror = function(event) {
      is(event.target, worker);
      ok(false, "Worker had an error: " + event.data);
      self.worker.terminate();
      finish();
    };

    worker.postMessage(0);
  }
};
