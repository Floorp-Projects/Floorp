const EXPECTED_CHAIN = [
  "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"
  ];

function handleRequest(request, response)
{
  if (request.queryString === "succeed") {
    // read the report from the client
    let inputStream = Components.classes["@mozilla.org/scriptableinputstream;1"].createInstance(Components.interfaces.nsIScriptableInputStream);
    inputStream.init(request.bodyInputStream, 0x01, 0004, 0);

    let body = "";
    if (inputStream) {
      while (inputStream.available()) {
        body = body + inputStream.read(inputStream.available());
      }
    }
    // parse the report
    let report = JSON.parse(body);
    let certChain = report.failedCertChain;

    // ensure the cert chain is what we expect
    for (idx in certChain) {
      if (certChain[idx] !== EXPECTED_CHAIN[idx]) {
        // if the chain differs, send an error response to cause test
        // failure
        response.setStatusLine("1.1", 500, "Server error");
        response.write("<html>The report contained an unexpected chain</html>");
        return;
      }
    }

    // if all is as expected, send the 201 the client expects
    response.setStatusLine("1.1", 201, "Created");
    response.write("<html>OK</html>");
  } else if (request.queryString === "error") {
    response.setStatusLine("1.1", 500, "Server error");
    response.write("<html>server error</html>");
  }
}
