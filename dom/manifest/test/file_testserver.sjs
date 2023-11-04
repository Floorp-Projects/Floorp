"use strict";
let { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

function loadHTMLFromFile(path) {
  // Load the HTML to return in the response from file.
  // Since it's relative to the cwd of the test runner, we start there and
  // append to get to the actual path of the file.
  const testHTMLFile = Services.dirsvc.get("CurWorkD", Ci.nsIFile);

  const testHTMLFileStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);

  path
    .split("/")
    .filter(path_1 => path_1)
    .reduce((file, path_2) => {
      testHTMLFile.append(path_2);
      return testHTMLFile;
    }, testHTMLFile);
  testHTMLFileStream.init(testHTMLFile, -1, 0, 0);
  const isAvailable = testHTMLFileStream.available();
  return NetUtil.readInputStreamToString(testHTMLFileStream, isAvailable);
}

function handleRequest(request, response) {
  const query = new URLSearchParams(request.queryString);

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // Deliver the CSP policy encoded in the URL
  if (query.has("csp")) {
    response.setHeader("Content-Security-Policy", query.get("csp"), false);
  }

  // Deliver the CSPRO policy encoded in the URL
  if (query.has("cspro")) {
    response.setHeader(
      "Content-Security-Policy-Report-Only",
      query.get("cspro"),
      false
    );
  }

  // Deliver the CORS header in the URL
  if (query.has("cors")) {
    response.setHeader("Access-Control-Allow-Origin", query.get("cors"), false);
  }

  // Send HTML to test allowed/blocked behaviors
  response.setHeader("Content-Type", "text/html", false);
  response.write(loadHTMLFromFile(query.get("file")));
}
