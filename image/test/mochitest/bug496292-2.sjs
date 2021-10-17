function handleRequest(request, response)
{
  var file = Components.classes["@mozilla.org/file/directory_service;1"]
             .getService(Components.interfaces.nsIProperties)
             .get("CurWorkD", Components.interfaces.nsIFile);

  file.append("tests");
  file.append("image");
  file.append("test");
  file.append("mochitest");

  if (request.getHeader("Accept") == "image/png") {
    file.append('blue.png');
  } else {
    file.append('red.png');
  }
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "image/png", false);
  response.setHeader("Cache-Control", "no-cache", false);

  var fileStream = Components.classes['@mozilla.org/network/file-input-stream;1']
                   .createInstance(Components.interfaces.nsIFileInputStream);
  fileStream.init(file, 1, 0, false);
  var binaryStream = Components.classes['@mozilla.org/binaryinputstream;1']
                     .createInstance(Components.interfaces.nsIBinaryInputStream);
  binaryStream.setInputStream(fileStream);

  response.bodyOutputStream.writeFrom(binaryStream, binaryStream.available());

  binaryStream.close();
  fileStream.close();
}
