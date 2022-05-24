function handleRequest(request, response) {
  var file = Services.dirsvc.get("CurWorkD", Ci.nsIFile);

  file.append("tests");
  file.append("image");
  file.append("test");
  file.append("mochitest");

  var filestate = "/image/test/mochitest/bug671906.sjs";
  if (getState(filestate) == "") {
    file.append("blue.png");
    setState(filestate, "red");
  } else {
    file.append("red.png");
    setState(filestate, "");
  }

  // Set the expires date to some silly time in the future so we're sure to
  // *want* to cache this image.
  var date = new Date();
  date.setFullYear(date.getFullYear() + 1);
  response.setHeader("Expires", date.toUTCString(), false);

  var fileStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  fileStream.init(file, 1, 0, false);

  response.bodyOutputStream.writeFrom(fileStream, fileStream.available());

  fileStream.close();

  response.setHeader("Access-Control-Allow-Origin", "*", false);
}
