const BinaryInputStream = Components.Constructor("@mozilla.org/binaryinputstream;1",
                                                 "nsIBinaryInputStream",
                                                 "setInputStream");

function handleRequest(request, response) {
  response.setHeader("Large-Allocation", "0", false);
  response.setHeader("Content-Type", "text/plain", false);
  response.setStatusLine(request.httpVersion, "200", "Found");
  if (request.method == "GET") {
    response.write("FAIL");
    return;
  }

  var body = new BinaryInputStream(request.bodyInputStream);

  var avail;
  var bytes = [];

  while ((avail = body.available()) > 0)
    Array.prototype.push.apply(bytes, body.readByteArray(avail));

  var data = String.fromCharCode.apply(null, bytes);
  response.bodyOutputStream.write(data, data.length);
}
