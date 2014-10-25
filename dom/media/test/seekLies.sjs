function handleRequest(request, response)
{
  var file = Components.classes["@mozilla.org/file/directory_service;1"].
                        getService(Components.interfaces.nsIProperties).
                        get("CurWorkD", Components.interfaces.nsILocalFile);
  var fis  = Components.classes['@mozilla.org/network/file-input-stream;1'].
                        createInstance(Components.interfaces.nsIFileInputStream);
  var bis  = Components.classes["@mozilla.org/binaryinputstream;1"].
                        createInstance(Components.interfaces.nsIBinaryInputStream);
  var paths = "tests/dom/media/test/seek.ogv";
  var split = paths.split("/");
  for(var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fis.init(file, -1, -1, false);
  bis.setInputStream(fis);
  var bytes = bis.readBytes(bis.available());
  response.setHeader("Content-Length", ""+bytes.length, false);
  response.setHeader("Content-Type", "video/ogg", false);
  response.setHeader("Accept-Ranges", "bytes", false);
  response.write(bytes, bytes.length);
  bis.close();
}
