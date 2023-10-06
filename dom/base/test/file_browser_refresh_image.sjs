"use strict";

function handleRequest(request, response) {
  const file = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIProperties)
    .get("CurWorkD", Ci.nsIFile);

  file.append("tests");
  file.append("image");
  file.append("test");
  file.append("mochitest");

  const redirectstate = "image_resource";
  if (getState(redirectstate) == "") {
    file.append("green.png");
    setState(redirectstate, "green");
  } else {
    file.append("red.png");
    setState(redirectstate, "");
  }

  response.setHeader("Cache-Control", "max-age=3600", false);

  const fileStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);

  fileStream.init(file, 1, 0, false);
  const binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  binaryStream.setInputStream(fileStream);

  response.bodyOutputStream.writeFrom(binaryStream, binaryStream.available());

  binaryStream.close();
  fileStream.close();
}
