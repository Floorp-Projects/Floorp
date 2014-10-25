const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function utf8decode(s) {
  return decodeURIComponent(escape(s));
}

function utf8encode(s) {
  return unescape(encodeURIComponent(s));
}

function handleRequest(request, response)
{
  var bodyStream = new BinaryInputStream(request.bodyInputStream);
  var result = [];
  var requestBody = "";
  while ((bodyAvail = bodyStream.available()) > 0)
    requestBody += bodyStream.readBytes(bodyAvail);

  if (request.method == "POST") {

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

    if (contentTypeParams[''] == "multipart/form-data" &&
        request.queryString == "") {
      requestBody.split("--" + contentTypeParams.boundary).slice(1, -1).forEach(function (s) {

        let headers = {};
        let headerEnd = s.indexOf("\r\n\r\n");
        s.substr(2, headerEnd-2).split("\r\n").forEach(function(s) {
          // We're assuming UTF8 for now
          let [name, value] = s.split(': ');
          headers[name] = utf8decode(value);
        });

        let body = s.substring(headerEnd + 4, s.length - 2);
        if (!headers["Content-Type"] || headers["Content-Type"] == "text/plain") {
          // We're assuming UTF8 for now
          body = utf8decode(body);
        }
        result.push({ headers: headers, body: body});
      });
    }
    if (contentTypeParams[''] == "text/plain" &&
        request.queryString == "plain") {
      result = utf8decode(requestBody);
    }
    if (contentTypeParams[''] == "application/x-www-form-urlencoded" &&
        request.queryString == "url") {
      result = requestBody;
    }
  }
  else if (request.method == "GET") {
    result = request.queryString;
  }

  // Send response body
  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);
  response.write(utf8encode(JSON.stringify(result)));
}
