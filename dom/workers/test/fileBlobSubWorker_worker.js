/**
 * Expects a blob. Returns an object containing the size, type.
 *  Used to test posting of blob from worker to worker.
 */
onmessage = function(event) {
  var worker = new Worker("fileBlob_worker.js");

  worker.postMessage(event.data);

  worker.onmessage = function(event) {
    postMessage(event.data);
  }

  worker.onerror = function(event) {
    postMessage(undefined);
  }
};
