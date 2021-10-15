const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");
const BinaryOutputStream = CC("@mozilla.org/binaryoutputstream;1",
                              "nsIBinaryOutputStream",
                              "setOutputStream");

function handleRequest(request, response) {
  var bodyStream = new BinaryInputStream(request.bodyInputStream);
  var bodyBytes = [];
  while ((bodyAvail = bodyStream.available()) > 0)
    Array.prototype.push.apply(bodyBytes, bodyStream.readByteArray(bodyAvail));

  var bos = new BinaryOutputStream(response.bodyOutputStream);
  bos.writeByteArray(bodyBytes, bodyBytes.length);
}
