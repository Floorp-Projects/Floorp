/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

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
  response.setHeader("Content-Type", "image/gif", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setStatusLine(request.httpVersion, 200, "OK");
  // We're sending data off in a delayed fashion
  response.processAsync();
  var inputStream = getFileAsInputStream("animated-gif_trailing-garbage.gif");
  var available = inputStream.available(); // = 4029 bytes
  // Send the good data at once
  response.bodyOutputStream.writeFrom(inputStream, 285);
  sendParts(inputStream, response);
}

function sendParts(inputStream, response) {
  // 3744 left, send in 8 chunks of 468 each
  partTimer.initWithCallback(getSendNextPart(inputStream, response), 500,
                             Components.interfaces.nsITimer.TYPE_ONE_SHOT);
}

function getSendNextPart(inputStream, response) {
  return function () {
    response.bodyOutputStream.writeFrom(inputStream, 468);
    if (!inputStream.available()) {
      inputStream.close();
      response.finish();
    } else {
      sendParts(inputStream, response);
    }
  };
}

