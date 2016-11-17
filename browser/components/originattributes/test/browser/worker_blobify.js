// Wait for a string to be posted to this worker.
// Create a blob containing this string, and then
// post back a blob URL pointing to the blob.
self.addEventListener("message", function(message) {
  try {
    var blobURL = URL.createObjectURL(new Blob([message.data]));
    postMessage({ blobURL });
  } catch (e) {
    postMessage({ error: e.message });
  }
}, false);
