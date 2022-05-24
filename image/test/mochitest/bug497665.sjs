function handleRequest(request, response) {
  var file = Services.dirsvc.get("CurWorkD", Ci.nsIFile);

  file.append("tests");
  file.append("image");
  file.append("test");
  file.append("mochitest");

  var redirectstate = "/image/test/mochitest/bug497665.sjs";
  if (getState(redirectstate) == "") {
    file.append("blue.png");
    setState(redirectstate, "red");
  } else {
    file.append("red.png");
    setState(redirectstate, "");
  }

  response.setHeader("Cache-Control", "max-age=3600", false);

  var fileStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  fileStream.init(file, 1, 0, false);
  var binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  binaryStream.setInputStream(fileStream);

  response.bodyOutputStream.writeFrom(binaryStream, binaryStream.available());

  binaryStream.close();
  fileStream.close();
}
