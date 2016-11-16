// Wait for a string to be posted to this worker.
// Create a blob containing this string, and then
// post back a blob URL pointing to the blob.
self.addEventListener("message", function (e) {
  try {
    var blobURL = URL.createObjectURL(new Blob([e.data]));
    postMessage({ blobURL });
  } catch (e) {
    postMessage({ error: e.message });
  }
}, false);
