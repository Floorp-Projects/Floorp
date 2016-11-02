const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/plain", false);

  if (request.method == "POST") {
    var body = new BinaryInputStream(request.bodyInputStream);

    var avail;
    var bytes = [];
    while ((avail = body.available()) > 0) {
      Array.prototype.push.apply(bytes, body.readByteArray(avail));
    }

    var data = String.fromCharCode.apply(null, bytes);
    setState("response", data);
    response.write("sure!");
  } else {
    response.write(getState("response"));
  }
}
