/**
 * Expects an object containing a blob, a start offset, an end offset
 * and an optional content type to slice the blob. Returns an object
 * containing the size and type of the sliced blob.
 */
onmessage = function(event) {
  var blob = event.data.blob;
  var start = event.data.start;
  var end = event.data.end;
  var contentType = event.data.contentType;

  var slicedBlob;
  if (contentType == undefined && end == undefined) {
    slicedBlob = blob.slice(start);
  } else if (contentType == undefined) {
    slicedBlob = blob.slice(start, end);
  } else {
    slicedBlob = blob.slice(start, end, contentType);
  }

  var rtnObj = new Object();

  rtnObj.size = slicedBlob.size;
  rtnObj.type = slicedBlob.type;

  postMessage(rtnObj);
};
