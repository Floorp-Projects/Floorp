// SJS file for CSP mochitests

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

function loadHTMLFromFile(path) {
  // Load the HTML to return in the response from file.
  // Since it's relative to the cwd of the test runner, we start there and
  // append to get to the actual path of the file.
  var testHTMLFile = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIProperties)
    .get("CurWorkD", Ci.nsIFile);
  var dirs = path.split("/");
  for (var i = 0; i < dirs.length; i++) {
    testHTMLFile.append(dirs[i]);
  }
  var testHTMLFileStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  testHTMLFileStream.init(testHTMLFile, -1, 0, 0);
  var testHTML = NetUtil.readInputStreamToString(
    testHTMLFileStream,
    testHTMLFileStream.available()
  );
  return testHTML;
}

function handleRequest(request, response) {
  var query = {};
  request.queryString.split("&").forEach(function (val) {
    var [name, value] = val.split("=");
    query[name] = unescape(value);
  });

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // Deliver the CSP policy encoded in the URI
  if (query.csp) {
    response.setHeader("Content-Security-Policy", unescape(query.csp), false);
  }

  // Send HTML to test allowed/blocked behaviors
  response.setHeader("Content-Type", "text/html", false);
  response.write(
    loadHTMLFromFile("tests/dom/security/test/csp/file_bug888172.html")
  );
}
