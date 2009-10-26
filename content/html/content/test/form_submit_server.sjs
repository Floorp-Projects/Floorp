const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response)
{
  var bodyStream = new BinaryInputStream(request.bodyInputStream);
  var bodyBytes = [];
  var result = [];
  while ((bodyAvail = bodyStream.available()) > 0)
    Array.prototype.push.apply(bodyBytes, bodyStream.readByteArray(bodyAvail));

  if (request.method == "POST") {
    // assume UTF8 for now
    var requestBody = decodeURIComponent(
      escape(String.fromCharCode.apply(null, bodyBytes)));

    var contentTypeParams = {};
    request.getHeader("Content-Type").split(/\s*\;\s*/).forEach(function(s) {
      if (s.indexOf('=') >= 0) {
        let [name, value] = s.split('=');
        contentTypeParams[name] = value;
      }
      else {
        contentTypeParams[''] = s;
      }
    });

    if (contentTypeParams[''] == "multipart/form-data") {
      requestBody.split("--" + contentTypeParams.boundary).slice(1, -1).forEach(function (s) {

        let headers = {};
        headerEnd = s.indexOf("\r\n\r\n");
        s.substr(2, headerEnd-2).split("\r\n").forEach(function(s) {
          let [name, value] = s.split(': ');
          headers[name] = value;
        });
	result.push({ headers: headers, body: s.substring(headerEnd + 4, s.length - 2)});
      });
    }
  }

  // Send response body
  response.setHeader("Content-Type", "text/plain", false);
  response.write(JSON.stringify(result));
}
