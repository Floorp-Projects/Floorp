/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
var seenScopeError;
onerror = function(message, filename, lineno) {
  if (!seenScopeError) {
    seenScopeError = true;
    postMessage({
      type: "scope",
      data: { message: message, filename: filename, lineno: lineno }
    });
    return true;
  }
};

onmessage = function(event) {
  var workerId = parseInt(event.data);

  if (workerId > 1) {
    var worker = new Worker("errorPropagation_worker.js");

    worker.onmessage = function(msg) {
      postMessage(msg.data);
    };

    var seenWorkerError;
    worker.onerror = function(error) {
      if (!seenWorkerError) {
        seenWorkerError = true;
        postMessage({
          type: "worker",
          data: {
            message: error.message,
            filename: error.filename,
            lineno: error.lineno
          }
        });
        error.preventDefault();
      }
    };

    worker.postMessage(workerId - 1);
    return;
  }

  var interval = setInterval(function() {
    throw new Error("expectedError");
  }, 100);
};
