// SJS file for CSP mochitests
"use strict";
const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

function loadHTMLFromFile(path) {
  // Load the HTML to return in the response from file.
  // Since it's relative to the cwd of the test runner, we start there and
  // append to get to the actual path of the file.
  const testHTMLFile = Cc["@mozilla.org/file/directory_service;1"]
    .getService(Ci.nsIProperties)
    .get("CurWorkD", Ci.nsIFile);

  const testHTMLFileStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);

  path
    .split("/")
    .filter(path => path)
    .reduce((file, path) => {
      testHTMLFile.append(path);
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

  // Deliver the CSP report-only policy encoded in the URI
  if (query.has("cspRO")) {
    response.setHeader(
      "Content-Security-Policy-Report-Only",
      query.get("cspRO"),
      false
    );
  }

  // Deliver the CORS header in the URL
  if (query.has("cors")) {
    response.setHeader("Access-Control-Allow-Origin", query.get("cors"), false);
  }

  // Send HTML to test allowed/blocked behaviors
  let type = "text/html";
  if (query.has("type")) {
    type = query.get("type");
  }

  response.setHeader("Content-Type", type, false);
  if (query.has("file")) {
    response.write(loadHTMLFromFile(query.get("file")));
  }
}
