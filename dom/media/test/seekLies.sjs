function handleRequest(request, response) {
  var file = Services.dirsvc.get("CurWorkD", Ci.nsIFile);
  var fis = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  var bis = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  var paths = "tests/dom/media/test/vp9.webm";
  var split = paths.split("/");
  for (var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fis.init(file, -1, -1, false);
  bis.setInputStream(fis);
  var bytes = bis.readBytes(bis.available());
  response.setHeader("Content-Length", "" + bytes.length, false);
  response.setHeader("Content-Type", "video/webm", false);
  response.setHeader("Accept-Ranges", "bytes", false);
  response.write(bytes, bytes.length);
  bis.close();
}
