const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response)
{
  if (request.hasHeader("Content-Type"))
    response.setHeader("Result-Content-Type",
                       request.getHeader("Content-Type"));

  response.setHeader("Content-Type", "text/plain; charset=ISO-8859-1");

  var body = new BinaryInputStream(request.bodyInputStream);
  var avail;
  var bytes = [];
  while ((avail = body.available()) > 0)
    Array.prototype.push.apply(bytes, body.readByteArray(avail));

  var data = String.fromCharCode.apply(null, bytes);
  response.setHeader("Result-Content-Length", "" + data.length);
  if (data.indexOf("TEST_REDIRECT_STR") >= 0) {
    var newURL = "http://" + data.split("&url=")[1];
    response.setStatusLine(null, 307, "redirect");
    response.setHeader("Location", newURL, false);
  }
  else {
    response.write(data);
  }
}
