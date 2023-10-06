function handleRequest(request, response) {
  var file = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIProperties)
    .get("CurWorkD", Ci.nsIFile);
  var fis = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  var bis = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  var paths = "tests/dom/media/webaudio/test/small-shot.ogg";
  var split = paths.split("/");
  for (var i = 0; i < split.length; ++i) {
    file.append(split[i]);
  }
  fis.init(file, -1, -1, false);
  bis.setInputStream(fis);
  var bytes = bis.readBytes(bis.available());
  response.setHeader("Content-Type", "video/ogg", false);
  response.setHeader("Content-Length", "" + bytes.length, false);
  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.write(bytes, bytes.length);
  response.processAsync();
  response.finish();
  bis.close();
}
