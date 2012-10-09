/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var bodyPartIndex = -1;
var bodyParts = [
  ["red.png", "image/png"],
  ["animated-gif2.gif", "image/gif"],
  ["red.png", "image/png"],
  ["lime100x100.svg", "image/svg+xml"],
  ["lime100x100.svg", "image/svg+xml"],
  ["animated-gif2.gif", "image/gif"],
  ["red.png", "image/png"],
  ["damon.jpg", "image/jpeg"],
  ["damon.jpg", "application/octet-stream"],
  ["damon.jpg", "image/jpeg"],
  ["rillybad.jpg", "application/x-unknown-content-type"],
  ["damon.jpg", "image/jpeg"],
  ["bad.jpg", "image/jpeg"],
  ["red.png", "image/png"],
  ["invalid.jpg", "image/jpeg"],
  ["animated-gif2.gif", "image/gif"]
];
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
  if (!getSharedState("next-part")) {
    setSharedState("next-part", "-1");
  }
  response.setHeader("Content-Type",
                     "multipart/x-mixed-replace;boundary=BOUNDARYOMG", false);
  response.setHeader("Cache-Control", "no-cache", false);
  response.setStatusLine(request.httpVersion, 200, "OK");
  // We're sending parts off in a delayed fashion, to let the tests occur.
  response.processAsync();
  response.write("--BOUNDARYOMG\r\n");
  sendParts(response);
}

function sendParts(response) {
  let wait = false;
  let nextPart = parseInt(getSharedState("next-part"), 10);
  if (nextPart == bodyPartIndex) {
    // Haven't been signaled yet, remain in holding pattern
    wait = true;
  } else {
    bodyPartIndex = nextPart;
  }
  if (bodyParts.length > bodyPartIndex) {
    let callback;
    if (!wait) {
      callback = getSendNextPart(response);
    } else {
      callback = function () { sendParts(response); };
    }
    partTimer.initWithCallback(callback, 1000,
                               Components.interfaces.nsITimer.TYPE_ONE_SHOT);
  }
  else {
    sendClose(response);
  }
}

function sendClose(response) {
  response.write("--BOUNDARYOMG--\r\n");
  response.finish();
}

function getSendNextPart(response) {
  var part = bodyParts[bodyPartIndex];
  var nextPartHead = "Content-Type: " + part[1] + "\r\n\r\n";
  var inputStream = getFileAsInputStream(part[0]);
  return function () {
    response.bodyOutputStream.write(nextPartHead, nextPartHead.length);
    response.bodyOutputStream.writeFrom(inputStream, inputStream.available());
    inputStream.close();
    // Toss in the boundary, so the browser can know this part is complete
    response.write("--BOUNDARYOMG\r\n");
    sendParts(response);
  }
}

