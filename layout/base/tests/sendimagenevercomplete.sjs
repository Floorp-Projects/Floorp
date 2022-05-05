function getFileStream(filename) {
  // Get the location of this sjs file, and then use that to figure out where
  // to find where our other files are.
  var self = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  self.initWithPath(getState("__LOCATION__"));
  var file = self.parent;
  file.append(filename);

  var fileStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  fileStream.init(file, 1, 0, false);

  return fileStream;
}

function handleRequest(request, response) {
  // partial.png is a truncated png
  // by calling processAsync and not calling finish we send the truncated png
  // in it's entirety but the webserver doesn't close to connection to indicate
  // that all data has been delivered.
  response.processAsync();
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "image/png", false);

  var inputStream = getFileStream("partial.png");
  response.bodyOutputStream.writeFrom(inputStream, inputStream.available());
  inputStream.close();
}
