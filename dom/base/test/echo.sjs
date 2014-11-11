const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/plain");
  if (request.method == "GET") {
     response.write(request.queryString);
     return;
  }

  var bodyStream = new BinaryInputStream(request.bodyInputStream);
  var body = "";
  var bodyAvail;
  while ((bodyAvail = bodyStream.available()) > 0)
    body += String.fromCharCode.apply(null, bodyStream.readByteArray(bodyAvail));
  
  response.write(body);
}
