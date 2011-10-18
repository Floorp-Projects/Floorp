function handleRequest(request, response)
{
  var file = Components.classes["@mozilla.org/file/directory_service;1"]
             .getService(Components.interfaces.nsIProperties)
             .get("CurWorkD", Components.interfaces.nsIFile);

  file.append("tests");
  file.append("image");
  file.append("test");
  file.append("mochitest");

  var filestate = "/image/test/mochitest/bug671906.sjs";
  if (getState(filestate) == "") {
    file.append('blue.png');
    setState(filestate, "red");
  } else {
    file.append('red.png');
    setState(filestate, "");
  }

  // Set the expires date to some silly time in the future so we're sure to
  // *want* to cache this image.
  var date = new Date();
  date.setFullYear(date.getFullYear() + 1);
  response.setHeader("Expires", date.toUTCString(), false);

  var fileStream = Components.classes['@mozilla.org/network/file-input-stream;1']
                   .createInstance(Components.interfaces.nsIFileInputStream);
  fileStream.init(file, 1, 0, false);

  response.bodyOutputStream.writeFrom(fileStream, fileStream.available());

  fileStream.close();

  response.setHeader("Access-Control-Allow-Origin", "*", false);
}
