// Utilities
// =========

// Helper that uses FileReader or FileReaderSync based on context and returns
// a Promise that resolves with the text or rejects with error.
function readAsText(blob) {
  if (typeof FileReader !== "undefined") {
    return new Promise(function(resolve, reject) {
      var fs = new FileReader();
      fs.onload = function() {
        resolve(fs.result);
      }
      fs.onerror = reject;
      fs.readAsText(blob);
    });
  } else {
    var fs = new FileReaderSync();
    return Promise.resolve(fs.readAsText(blob));
  }
}

function readAsArrayBuffer(blob) {
  if (typeof FileReader !== "undefined") {
    return new Promise(function(resolve, reject) {
      var fs = new FileReader();
      fs.onload = function() {
        resolve(fs.result);
      }
      fs.onerror = reject;
      fs.readAsArrayBuffer(blob);
    });
  } else {
    var fs = new FileReaderSync();
    return Promise.resolve(fs.readAsArrayBuffer(blob));
  }
}

function waitForState(worker, state, context) {
  return new Promise(resolve => {
    if (worker.state === state) {
      resolve(context);
      return;
    }
    worker.addEventListener('statechange', function onStateChange() {
      if (worker.state === state) {
        worker.removeEventListener('statechange', onStateChange);
        resolve(context);
      }
    });
  });
}
