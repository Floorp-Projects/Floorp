"use strict";

function handleRequest(request, response) {
  const file = Components.classes["@mozilla.org/file/directory_service;1"]
    .getService(Components.interfaces.nsIProperties)
    .get("CurWorkD", Components.interfaces.nsIFile);

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

  const fileStream = Components.classes[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Components.interfaces.nsIFileInputStream);

  fileStream.init(file, 1, 0, false);
  const binaryStream = Components.classes[
    "@mozilla.org/binaryinputstream;1"
  ].createInstance(Components.interfaces.nsIBinaryInputStream);
  binaryStream.setInputStream(fileStream);

  response.bodyOutputStream.writeFrom(binaryStream, binaryStream.available());

  binaryStream.close();
  fileStream.close();
}
