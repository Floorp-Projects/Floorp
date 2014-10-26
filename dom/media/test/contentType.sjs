// Parse the query string, and give us the value for a certain key, or false if
// it does not exist.
function parseQuery(request, key) {
  var params = request.queryString.split('?')[0].split('&');
  for (var j = 0; j < params.length; ++j) {
    var p = params[j];
    if (p == key)
      return true;
    if (p.indexOf(key + "=") == 0)
      return p.substring(key.length + 1);
    if (p.indexOf("=") < 0 && key == "")
      return p;
  }
  return false;
}

function handleRequest(request, response) {
  try {
    // Get the filename to send back.
    var filename = parseQuery(request, "file");

    const CC = Components.Constructor;
    const BinaryOutputStream = CC("@mozilla.org/binaryoutputstream;1",
                                  "nsIBinaryOutputStream",
                                  "setOutputStream");
    var file = Components.classes["@mozilla.org/file/directory_service;1"].
      getService(Components.interfaces.nsIProperties).
      get("CurWorkD", Components.interfaces.nsILocalFile);
    var fis  = Components.classes['@mozilla.org/network/file-input-stream;1'].
      createInstance(Components.interfaces.nsIFileInputStream);
    var bis  = Components.classes["@mozilla.org/binaryinputstream;1"].
      createInstance(Components.interfaces.nsIBinaryInputStream);
    var paths = "tests/dom/media/test/" + filename;
    dump(paths + '\n');
    var split = paths.split("/");
    for(var i = 0; i < split.length; ++i) {
      file.append(split[i]);
    }
    fis.init(file, -1, -1, false);

    // handle range requests
    var partialstart = 0,
        partialend = file.fileSize - 1;
    if (request.hasHeader("Range")) {
      var range = request.getHeader("Range");
      var parts = range.replace(/bytes=/, "").split("-");
      var partialstart = parts[0];
      var partialend = parts[1];
      if (!partialend.length) {
        partialend = file.fileSize - 1;
      }
      response.setStatusLine(request.httpVersion, 206, "Partial Content");
      var contentRange = "bytes " + partialstart + "-" + partialend + "/" + file.fileSize;
      response.setHeader("Content-Range", contentRange);
    }

    fis.seek(Components.interfaces.nsISeekableStream.NS_SEEK_SET, partialstart);
    bis.setInputStream(fis);

    var sendContentType = parseQuery(request, "nomime");
    if (sendContentType == false) {
      var contentType = parseQuery(request, "type");
      if (contentType == false) {
        // This should not happen.
        dump("No type specified without having \'nomime\' in parameters.");
        return;
      }
      response.setHeader("Content-Type", contentType, false);
    }
    response.setHeader("Content-Length", ""+bis.available(), false);

    var bytes = bis.readBytes(bis.available());
    response.write(bytes, bytes.length);
  } catch (e) {
    dump ("ERROR : " + e + "\n");
  }
}
