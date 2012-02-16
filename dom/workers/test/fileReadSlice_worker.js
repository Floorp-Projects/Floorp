/**
 * Expects an object containing a blob, a start index and an end index
 * for slicing. Returns the contents of the blob read as text.
 */
onmessage = function(event) {
  var blob = event.data.blob;
  var start = event.data.start;
  var end = event.data.end;

  var slicedBlob = blob.slice(start, end);

  var fileReader = new FileReaderSync();
  var text = fileReader.readAsText(slicedBlob);

  postMessage(text);
};
