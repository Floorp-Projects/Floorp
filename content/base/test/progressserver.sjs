const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function setReq(req) {
  setObjectState("content/base/test/progressserver", req);
}

function getReq() {
  var req;
  getObjectState("content/base/test/progressserver", function(v) {
    req = v;
  });
  return req;
}

function handleRequest(request, response)
{
  var pairs = request.queryString.split('&');
  var command = pairs.shift();

  var bodyStream = new BinaryInputStream(request.bodyInputStream);
  var body = "";
  var bodyAvail;
  while ((bodyAvail = bodyStream.available()) > 0)
    body += String.fromCharCode.apply(null, bodyStream.readByteArray(bodyAvail));

  if (command == "open") {
    response.processAsync();
    setReq(response);

    response.setHeader("Cache-Control", "no-cache", false);
    pairs.forEach(function (val) {
      var [name, value] = val.split('=');
      response.setHeader(name, unescape(value), false);
    });
    response.write(body);
    return;
  }

  if (command == "send") {
    getReq().write(body);
  }
  else if (command == "close") {
    getReq().finish();
    setReq(null);
  }
  response.setHeader("Content-Type", "text/plain");
  response.write("ok");
}
