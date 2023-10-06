// Custom *.sjs specifically for the needs of Bug
// Bug 1139297 - Implement CSP upgrade-insecure-requests directive

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

const REPORT_URI =
  "https://example.com/tests/dom/security/test/csp/file_upgrade_insecure_reporting_server.sjs?report";
const POLICY = "upgrade-insecure-requests; default-src https: 'unsafe-inline'";
const POLICY_RO =
  "default-src https: 'unsafe-inline'; report-uri " + REPORT_URI;

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
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // (1) Store the query that will report back whether the violation report was received
  if (request.queryString == "queryresult") {
    response.processAsync();
    setObjectState("queryResult", response);
    return;
  }

  // (2) We load a page using a CSP and a report only CSP
  if (request.queryString == "toplevel") {
    response.setHeader("Content-Security-Policy", POLICY, false);
    response.setHeader("Content-Security-Policy-Report-Only", POLICY_RO, false);
    response.setHeader("Content-Type", "text/html", false);
    response.write(
      loadHTMLFromFile(
        "tests/dom/security/test/csp/file_upgrade_insecure_reporting.html"
      )
    );
    return;
  }

  // (3) Return the image back to the client
  if (request.queryString == "img") {
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  // (4) Finally we receive the report, let's return the request from (1)
  // signaling that we received the report correctly
  if (request.queryString == "report") {
    getObjectState("queryResult", function (queryResponse) {
      if (!queryResponse) {
        return;
      }
      queryResponse.write("report-ok");
      queryResponse.finish();
    });
    return;
  }

  // we should never get here, but just in case ...
  response.setHeader("Content-Type", "text/plain");
  response.write("doh!");
}
