const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  var bodyStream = new BinaryInputStream(request.bodyInputStream);
  var bodyBytes = [];
  while ((bodyAvail = bodyStream.available()) > 0)
    Array.prototype.push.apply(bodyBytes, bodyStream.readByteArray(bodyAvail));

  var body = decodeURIComponent(
    escape(String.fromCharCode.apply(null, bodyBytes)));

  var currentHop = query.hop ? parseInt(query.hop) : 0;

  var obj = JSON.parse(body);
  if (currentHop < obj.hops) {
    var newURL = '/tests/dom/workers/test/serviceworkers/redirect_post.sjs?hop=' +
                 (1 + currentHop);
    response.setStatusLine(null, 307, 'redirect');
    response.setHeader('Location', newURL);
    return;
  }

  response.setHeader('Content-Type', 'application/json');
  response.write(body);
}
