/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var counter = 2;
var frames = ['red.gif', 'blue.gif'];
var timer = Components.classes["@mozilla.org/timer;1"];
var partTimer = timer.createInstance(Components.interfaces.nsITimer);

function getFileAsInputStream(aFilename) {
  var file = Components.classes["@mozilla.org/file/directory_service;1"]
             .getService(Components.interfaces.nsIProperties)
             .get("CurWorkD", Components.interfaces.nsIFile);

  file.append("tests");
  file.append("image");
  file.append("test");
  file.append("mochitest");
  file.append(aFilename);

  var fileStream = Components.classes['@mozilla.org/network/file-input-stream;1']
                   .createInstance(Components.interfaces.nsIFileInputStream);
  fileStream.init(file, 1, 0, false);
  return fileStream;
}

function handleRequest(request, response)
{
  response.setHeader("Content-Type",
                     "multipart/x-mixed-replace;boundary=BOUNDARYOMG", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.processAsync();
  response.write("--BOUNDARYOMG\r\n");
  while (frames.length > 0) {
    sendNextPart(response);
  }
  response.write("--BOUNDARYOMG--\r\n");
  response.finish();
}

function sendNextPart(response) {
  var nextPartHead = "Content-Type: image/gif\r\n\r\n";
  var inputStream = getFileAsInputStream(frames.shift());
  response.bodyOutputStream.write(nextPartHead, nextPartHead.length);
  response.bodyOutputStream.writeFrom(inputStream, inputStream.available());
  inputStream.close();
  // Toss in the boundary, so the browser can know this part is complete
  response.write("--BOUNDARYOMG\r\n");
}

