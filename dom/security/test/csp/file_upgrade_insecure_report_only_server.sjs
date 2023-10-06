// Custom *.sjs specifically for the needs of Bug 1832249 - Consider report-only flag when upgrading insecure requests

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

const POLICY_CSP =
  "upgrade-insecure-requests; default-src https: 'unsafe-inline'";
const POLICY_CSP_RO =
  "default-src https: 'unsafe-inline'; upgrade-insecure-requests; report-uri https://example.com/tests/dom/security/test/csp/file_upgrade_insecure_report_only_server.sjs?report";

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

  // (1) Store the query that will report back whether the violation report was
  // received
  if (request.queryString.startsWith("queryresult-")) {
    let route = request.queryString.substring("queryresult-".length);
    response.processAsync();
    setObjectState(`queryResult-${route}`, response);
    return;
  }

  // (2) We load a page using a report only CSP
  if (request.queryString.endsWith("=true")) {
    let route = request.queryString.split("=")[0];
    if (route === "enforce") {
      response.setHeader("Content-Security-Policy", POLICY_CSP, false);
    }
    response.setHeader(
      "Content-Security-Policy-Report-Only",
      POLICY_CSP_RO + "-" + route,
      false
    );
    response.setHeader("Content-Type", "text/html", false);
    response.write(
      loadHTMLFromFile(
        "tests/dom/security/test/csp/file_upgrade_insecure_report_only.html"
      )
    );
    return;
  }

  // (3a) Return the image back to the client if http and refuse if https for
  // report only image
  if (request.queryString.startsWith("img-")) {
    let route = request.queryString.substring("img-".length);
    if (
      (request.scheme === "http" && route === "reportonly") ||
      (request.scheme === "https" && route === "enforce")
    ) {
      response.setHeader("Content-Type", "image/png");
      response.write(IMG_BYTES);
      return;
    } else {
      response.setStatusLine(metadata.httpVersion, 404, "NO");
      response.write("Aww, 404, I wanted a peanut.");
      return;
    }
  }

  // (4) Once we receive the report send it to the client via the saved
  // queryresult response object
  if (request.queryString.startsWith("report-")) {
    let route = request.queryString.substring("report-".length);
    getObjectState(`queryResult-${route}`, function (queryResponse) {
      if (!queryResponse) {
        return;
      }
      queryResponse.setHeader("Content-Type", "application/json");
      queryResponse.write(
        NetUtil.readInputStreamToString(
          request.bodyInputStream,
          request.bodyInputStream.available()
        )
      );
      queryResponse.finish();
    });
    return;
  }

  // we should never get here, but just in case ...
  response.setHeader("Content-Type", "text/plain");
  response.write("doh!");
}
