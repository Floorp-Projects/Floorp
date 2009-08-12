const Cc = Components.classes;
const Ci = Components.interfaces;
const TIMEOUT_INTERVAL_MS = 100;

// Global Context
var ctx = {};

function handleRequest(request, response) {

  // Allow us to asynchronously construct the response with timeouts
  // rather than forcing us to make the whole thing in one call. See
  // bug 396226.
  response.processAsync();

  // Figure out whether the client wants to load the image, or just
  // to tell us that it's ready for us to finish
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });
  if (query["continue"] == "true") {
    setState("doContinue", "yes");
    response.finish();
    return;
  }

  // We're serving up a png
  response.setHeader("Content-Type", "image/png", false);

  // Get the output stream
  ctx.ostream = response.bodyOutputStream;

  // Ugly hack, but effective - copied from content/media/test/contentDuration1.sjs
  var pngFile = Components.classes["@mozilla.org/file/directory_service;1"].
                           getService(Components.interfaces.nsIProperties).
                           get("CurWorkD", Components.interfaces.nsILocalFile);
  var paths = "tests/layout/generic/test/file_Dolske.png";
  var split = paths.split("/");
  for(var i = 0; i < split.length; ++i) {
    pngFile.append(split[i]);
  }

  // Get an input stream for the png data
  ctx.istream = Cc["@mozilla.org/network/file-input-stream;1"].
                      createInstance(Components.interfaces.nsIFileInputStream);
  ctx.istream.init(pngFile, -1, 0, 0);

  // Write the first 10 bytes, which is just boilerplate/magic bytes
  ctx.ostream.writeFrom(ctx.istream, 10);

  // Mark that we haven't yet been instructed to continue
  setState("doContinue", "no");

  // Wait for the continue request, then finish
  waitForContinueAndFinish();
}

function waitForContinueAndFinish() {

  // If we can continue
  if (getState("doContinue") == "yes")
    return finishRequest();

  // Wait 100 ms and check again
  var timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(waitForContinueAndFinish,
                         TIMEOUT_INTERVAL_MS, Ci.nsITimer.TYPE_ONE_SHOT);
}


function finishRequest() {

  // Write the rest of the data
  ctx.ostream.writeFrom(ctx.istream, ctx.istream.available());

  // Close the streams
  ctx.ostream.close();
  ctx.istream.close();

  // Finish off the response
  response.finish();
}
