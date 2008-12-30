var worker = new Worker("errorPropagation_worker2.js");

var errorCount = 0;
worker.onerror = function(event) {
  switch (errorCount++) {
    case 0:
    case 1:
      // Let it propagate.
      break;
    case 2:
      // Stop and rethrow.
      event.preventDefault();
      throw event.data;
      break;
    case 3:
      event.preventDefault();
      postMessage(event.data);
      worker.onerror = null;
      break;
    default:
  }
};

onmessage = function(event) {
  worker.postMessage(event.data);
};
