"use strict";

function gzipCompressString(string, obs) {
  const scs = Cc["@mozilla.org/streamConverters;1"].getService(
    Ci.nsIStreamConverterService
  );
  const listener = Cc["@mozilla.org/network/stream-loader;1"].createInstance(
    Ci.nsIStreamLoader
  );
  listener.init(obs);
  const converter = scs.asyncConvertData(
    "uncompressed",
    "gzip",
    listener,
    null
  );
  const stringStream = Cc[
    "@mozilla.org/io/string-input-stream;1"
  ].createInstance(Ci.nsIStringInputStream);
  stringStream.data = string;
  converter.onStartRequest(null, null);
  converter.onDataAvailable(null, stringStream, 0, string.length);
  converter.onStopRequest(null, null, null);
}

const ORIGINAL_JS_CONTENT = `console.log("original javascript content");`;

function handleRequest(request, response) {
  response.processAsync();

  // Generate data
  response.setHeader("Content-Type", "application/javascript", false);
  response.setHeader("Content-Encoding", "gzip", false);

  const observer = {
    onStreamComplete(loader, context, status, length, result) {
      const buffer = String.fromCharCode.apply(this, result);
      response.setHeader("Content-Length", "" + buffer.length, false);
      response.write(buffer);
      response.finish();
    },
  };
  gzipCompressString(ORIGINAL_JS_CONTENT, observer);
}
