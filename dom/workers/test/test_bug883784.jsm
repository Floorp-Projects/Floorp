this.EXPORTED_SYMBOLS = ["Test"];

this.Test = {
  start: function(ok, is, finish) {
    let worker = new ChromeWorker("url_worker.js");
    worker.onmessage = function(event) {

      if (event.data.type == 'finish') {
        finish();
      } else if (event.data.type == 'status') {
        ok(event.data.status, event.data.msg);
      } else if (event.data.type == 'url') {
        var xhr = Components.classes["@mozilla.org/xmlextras/xmlhttprequest;1"]
                  .createInstance(Components.interfaces.nsIXMLHttpRequest);
        xhr.open('GET', event.data.url, false);
        xhr.onreadystatechange = function() {
          if (xhr.readyState == 4) {
            ok(true, "Blob readable!");
          }
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

    worker.postMessage(true);
  }
};
