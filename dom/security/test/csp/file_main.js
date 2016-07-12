function doXHR(uri, callback) {
  try {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", uri);
    xhr.responseType = "blob";
    xhr.send();
    xhr.onload = function () {
      if (callback) callback(xhr.response);
    }
  } catch(ex) {}
}

doXHR("http://mochi.test:8888/tests/dom/security/test/csp/file_CSP.sjs?testid=xhr_good");
doXHR("http://example.com/tests/dom/security/test/csp/file_CSP.sjs?testid=xhr_bad");
fetch("http://mochi.test:8888/tests/dom/security/test/csp/file_CSP.sjs?testid=fetch_good");
fetch("http://example.com/tests/dom/security/test/csp/file_CSP.sjs?testid=fetch_bad");
navigator.sendBeacon("http://mochi.test:8888/tests/dom/security/test/csp/file_CSP.sjs?testid=beacon_good");
navigator.sendBeacon("http://example.com/tests/dom/security/test/csp/file_CSP.sjs?testid=beacon_bad");

var topWorkerBlob;
var nestedWorkerBlob;

doXHR("file_main_worker.js", function (topResponse) {
  topWorkerBlob = URL.createObjectURL(topResponse);
  doXHR("file_child_worker.js", function (response) {
    nestedWorkerBlob = URL.createObjectURL(response);
    runWorker();
  });
});

function runWorker() {
  // Top level worker, no subworker
  // Worker does not inherit CSP from owner document
  new Worker("file_main_worker.js").postMessage({inherited : "none"});

  // Top level worker, no subworker
  // Worker inherits CSP from owner document
  new Worker(topWorkerBlob).postMessage({inherited : "document"});

  // Subworker
  // Worker does not inherit CSP from parent worker
  new Worker("file_main_worker.js").postMessage({inherited : "none", nested : nestedWorkerBlob});

  // Subworker
  // Worker inherits CSP from parent worker
  new Worker("file_main_worker.js").postMessage({inherited : "parent", nested : nestedWorkerBlob});

  // Subworker
  // Worker inherits CSP from owner document -> parent worker -> subworker
  new Worker(topWorkerBlob).postMessage({inherited : "document", nested : nestedWorkerBlob});
}
