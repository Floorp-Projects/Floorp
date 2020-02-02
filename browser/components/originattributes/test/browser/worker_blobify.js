// Wait for a string to be posted to this worker.
// Create a blob containing this string, and then
// post back a blob URL pointing to the blob.

/* eslint-env worker */

var postStringInBlob = function(blobObject) {
  var fileReader = new FileReaderSync();
  var result = fileReader.readAsText(blobObject);
  postMessage(result);
};

self.addEventListener("message", e => {
  if (e.data.what === "blobify") {
    try {
      var blobURL = URL.createObjectURL(new Blob([e.data.message]));
      postMessage({ blobURL });
    } catch (e) {
      postMessage({ error: e.message });
    }
    return;
  }

  if (e.data.what === "deblobify") {
    if ("error" in e.data.message) {
      postMessage(e.data.message);
      return;
    }
    var blobURL = e.data.message.blobURL,
      xhr = new XMLHttpRequest();
    try {
      xhr.open("GET", blobURL, true);
      xhr.onload = function() {
        postStringInBlob(xhr.response);
      };
      xhr.onerror = function() {
        postMessage({ error: "xhr error" });
      };
      xhr.responseType = "blob";
      xhr.send();
    } catch (e) {
      postMessage({ error: e.message });
    }

    return;
  }

  postMessage("Invalid operation!");
});
