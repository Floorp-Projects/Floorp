// SJS file for CSP mochitests

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

  var csp = unescape(query['csp']);
  var file = unescape(query['file']);

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // Deliver the CSP policy encoded in the URI
  response.setHeader("Content-Security-Policy", csp, false);

  // Send HTML to test allowed/blocked behaviors
  response.setHeader("Content-Type", "text/html", false);
  response.write(loadHTMLFromFile(file));
}
