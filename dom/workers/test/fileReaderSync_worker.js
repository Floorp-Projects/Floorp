var reader = new FileReaderSync();

/**
 * Expects an object containing a file and an encoding then uses a
 * FileReaderSync to read the file. Returns an object containing the
 * file read a binary string, text, url and ArrayBuffer.
 */
onmessage = function(event) {
  var file = event.data.file;
  var encoding = event.data.encoding;

  var rtnObj = new Object();

  if (encoding != undefined) {
    rtnObj.text = reader.readAsText(file, encoding);
  } else {
    rtnObj.text = reader.readAsText(file);
  }

  rtnObj.bin = reader.readAsBinaryString(file);
  rtnObj.url = reader.readAsDataURL(file);
  rtnObj.arrayBuffer = reader.readAsArrayBuffer(file);

  postMessage(rtnObj);
};
