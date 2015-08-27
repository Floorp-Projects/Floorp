// custom *.sjs file specifically for the needs of:
// * Bug 1004703 - ignore 'unsafe-inline' if nonce- or hash-source specified
// * Bug 1198422: should not block inline script if default-src is not specified

Components.utils.import("resource://gre/modules/NetUtil.jsm");

function loadHTMLFromFile(path) {
  // Load the HTML to return in the response from file.
  // Since it's relative to the cwd of the test runner, we start there and
  // append to get to the actual path of the file.
  var testHTMLFile =
    Components.classes["@mozilla.org/file/directory_service;1"].
    getService(Components.interfaces.nsIProperties).
    get("CurWorkD", Components.interfaces.nsILocalFile);
  var dirs = path.split("/");
  for (var i = 0; i < dirs.length; i++) {
    testHTMLFile.append(dirs[i]);
  }
  var testHTMLFileStream =
    Components.classes["@mozilla.org/network/file-input-stream;1"].
    createInstance(Components.interfaces.nsIFileInputStream);
  testHTMLFileStream.init(testHTMLFile, -1, 0, 0);
  var testHTML = NetUtil.readInputStreamToString(testHTMLFileStream, testHTMLFileStream.available());
  return testHTML;
}


function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  var csp1 = (query['csp1']) ? unescape(query['csp1']) : "";
  var csp2 = (query['csp2']) ? unescape(query['csp2']) : "";
  var file = unescape(query['file']);

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // deliver the CSP encoded in the URI
  // please note that comma separation of two policies
  // acts like sending *two* separate policies
  var csp = csp1;
  if (csp2 !== "") {
    csp += ", " + csp2;
  }
  response.setHeader("Content-Security-Policy", csp, false);

  // Send HTML to test allowed/blocked behaviors
  response.setHeader("Content-Type", "text/html", false);

  response.write(loadHTMLFromFile(file));
}
