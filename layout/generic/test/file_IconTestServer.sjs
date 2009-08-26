const Cc = Components.classes;
const Ci = Components.interfaces;
const TIMEOUT_INTERVAL_MS = 100;

function handleRequest(request, response) {

  // Allow us to asynchronously construct the response with timeouts
  // rather than forcing us to make the whole thing in one call. See
  // bug 396226.
  response.processAsync();

  // Figure out whether the client wants to load the image, or just
  // to tell us to finish the previous load
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });
  if (query["continue"] == "true") {

    // Debugging information so we can figure out the hang
    dump("file_IconTestServer.js DEBUG - Got continue command\n");

    // Get the context structure and finish the old request
    getObjectState("context", function(obj) {

      // magic or goop, depending on how you look at it
      savedCtx = obj.wrappedJSObject;

      // Write the rest of the data
      savedCtx.ostream.writeFrom(savedCtx.istream, savedCtx.istream.available());

      // Close the streams
      savedCtx.ostream.close();
      savedCtx.istream.close();

      // Finish off 'the old response'
      savedCtx.response.finish();
    });

    // Finish off 'the current response'
    response.finish();
    return;
  }

  // Debugging information so we can figure out the hang
  dump("file_IconTestServer.js DEBUG - Got initial request\n");

  // Context structure - we need to set this up properly to pass to setObjectState
  var ctx = {
    QueryInterface: function(iid) {
      if (iid.equals(Components.interfaces.nsISupports))
        return this;
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
  };
  ctx.wrappedJSObject = ctx;

  // Save the response
  ctx.response = response;

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

  // Save the context structure for retrieval when we get pinged
  setObjectState("context", ctx);

  // Now we play the waiting game...

  // Debugging information so we can figure out the hang
  dump("file_IconTestServer.js DEBUG - Playing the waiting game\n");
}

