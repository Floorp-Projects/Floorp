const workerURL = 'http://mochi.test:8888/tests/dom/workers/test/importScripts_3rdParty_worker.js';

onmessage = function(a) {
  if (a.data.nested) {
    var worker = new Worker(workerURL);
    worker.onmessage = function(event) {
      postMessage(event.data);
    }

    worker.onerror = function(event) {
      event.preventDefault();
      postMessage({ error: event instanceof ErrorEvent &&
                           event.filename == workerURL });
    }

    a.data.nested = false;
    worker.postMessage(a.data);
    return;
  }

  // This first URL will use the same origin of this script.
  var sameOriginURL = new URL(a.data.url);
  var fileName1 = 42;

  // This is cross-origin URL.
  var crossOriginURL = new URL(a.data.url);
  crossOriginURL.host = 'example.com';
  crossOriginURL.port = 80;
  var fileName2 = 42;

  if (a.data.test == 'none') {
    importScripts(crossOriginURL.href);
    return;
  }

  try {
    importScripts(sameOriginURL.href);
  } catch(e) {
    if (!(e instanceof SyntaxError)) {
      postMessage({ result: false });
      return;
    }

    fileName1 = e.fileName;
  }

  if (fileName1 != sameOriginURL.href || !fileName1) {
    postMessage({ result: false });
    return;
  }

  if (a.data.test == 'try') {
    var exception;
    try {
      importScripts(crossOriginURL.href);
    } catch(e) {
      fileName2 = e.filename;
      exception = e;
    }

    postMessage({ result: fileName2 == workerURL &&
                          exception.name == "NetworkError" &&
                          exception.code == DOMException.NETWORK_ERR });
    return;
  }

  if (a.data.test == 'eventListener') {
    addEventListener("error", function(event) {
      event.preventDefault();
      postMessage({result: event instanceof ErrorEvent &&
                           event.filename == workerURL });
    });
  }

  if (a.data.test == 'onerror') {
    onerror = function(...args) {
      postMessage({result: args[1] == workerURL });
    }
  }

  importScripts(crossOriginURL.href);
}
