function gzipCompressString(string, obs) {
  let scs = Cc["@mozilla.org/streamConverters;1"].getService(
    Ci.nsIStreamConverterService
  );
  let listener = Cc["@mozilla.org/network/stream-loader;1"].createInstance(
    Ci.nsIStreamLoader
  );
  listener.init(obs);
  let converter = scs.asyncConvertData("uncompressed", "gzip", listener, null);
  let stringStream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stringStream.data = string;
  converter.onStartRequest(null, null);
  converter.onDataAvailable(null, stringStream, 0, string.length);
  converter.onStopRequest(null, null, null);
}

function produceData() {
  var chars =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()_+";
  var result = "";
  for (var i = 0; i < 100000; ++i) {
    result += chars;
  }
  return result;
}

function handleRequest(request, response) {
  response.processAsync();

  // Generate data
  var strings_to_send = produceData();
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Encoding", "gzip", false);

  let observer = {
    onStreamComplete(loader, context, status, length, result) {
      buffer = String.fromCharCode.apply(this, result);
      response.setHeader("Content-Length", "" + buffer.length, false);
      response.write(buffer);
      response.finish();
    },
  };
  gzipCompressString(strings_to_send, observer);
}
