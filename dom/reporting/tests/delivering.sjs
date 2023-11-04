const CC = Components.Constructor;
const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

function handleRequest(aRequest, aResponse) {
  var params = new URLSearchParams(aRequest.queryString);

  // Report-to setter
  if (aRequest.method == "GET" && params.get("task") == "header") {
    let extraParams = [];

    if (params.has("410")) {
      extraParams.push("410=true");
    }

    if (params.has("worker")) {
      extraParams.push("worker=true");
    }

    let body = {
      max_age: 1,
      endpoints: [
        {
          url:
            "https://example.org/tests/dom/reporting/tests/delivering.sjs" +
            (extraParams.length ? "?" + extraParams.join("&") : ""),
          priority: 1,
          weight: 1,
        },
      ],
    };

    aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");
    aResponse.setHeader("Report-to", JSON.stringify(body), false);
    aResponse.write("OK");
    return;
  }

  // Report check
  if (aRequest.method == "GET" && params.get("task") == "check") {
    aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");

    let reports = getState("report");
    if (!reports) {
      aResponse.write("");
      return;
    }

    if (params.has("min")) {
      let json = JSON.parse(reports);
      if (json.length < params.get("min")) {
        aResponse.write("");
        return;
      }
    }

    aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");
    aResponse.write(getState("report"));

    setState("report", "");
    return;
  }

  if (aRequest.method == "POST") {
    var body = new BinaryInputStream(aRequest.bodyInputStream);

    var avail;
    var bytes = [];
    while ((avail = body.available()) > 0) {
      Array.prototype.push.apply(bytes, body.readByteArray(avail));
    }

    let reports = getState("report");
    if (!reports) {
      reports = [];
    } else {
      reports = JSON.parse(reports);
    }

    const incoming_reports = JSON.parse(String.fromCharCode.apply(null, bytes));
    for (let report of incoming_reports) {
      let data = {
        contentType: aRequest.getHeader("content-type"),
        origin: aRequest.getHeader("origin"),
        body: report,
        url:
          aRequest.scheme +
          "://" +
          aRequest.host +
          aRequest.path +
          (aRequest.queryString ? "&" + aRequest.queryString : ""),
      };
      reports.push(data);
    }

    setState("report", JSON.stringify(reports));

    if (params.has("410")) {
      aResponse.setStatusLine(aRequest.httpVersion, 410, "Gone");
    } else {
      aResponse.setStatusLine(aRequest.httpVersion, 200, "OK");
    }
    return;
  }

  aResponse.setStatusLine(aRequest.httpVersion, 500, "Internal error");
  aResponse.write("Invalid request");
}
