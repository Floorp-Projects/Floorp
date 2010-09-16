function handleRequest(request, response)
{
  var referer = request.hasHeader("Referer") ? request.getHeader("Referer")
                                             : undefined; 
  if (referer == "http://mochi.test:8888/tests/content/media/test/test_referer.html") {
    var [ignore, name, type] = request.queryString.match(/name=(.*)&type=(.*)$/);
    var file = Components.classes["@mozilla.org/file/directory_service;1"].
                          getService(Components.interfaces.nsIProperties).
                          get("CurWorkD", Components.interfaces.nsILocalFile);
    var fis  = Components.classes['@mozilla.org/network/file-input-stream;1'].
                          createInstance(Components.interfaces.nsIFileInputStream);
    var bis  = Components.classes["@mozilla.org/binaryinputstream;1"].
                          createInstance(Components.interfaces.nsIBinaryInputStream);
    var paths = "tests/content/media/test/" + name;
    var split = paths.split("/");
    for(var i = 0; i < split.length; ++i) {
      file.append(split[i]);
    }
    fis.init(file, -1, -1, false);
    bis.setInputStream(fis);
    var bytes = bis.readBytes(bis.available());
    response.setHeader("Content-Length", ""+bytes.length, false);
    response.setHeader("Content-Type", type, false);
    response.write(bytes, bytes.length);
    bis.close();
  }
  else {
    response.setStatusLine(request.httpVersion, 404, "Not found");
  }
}
