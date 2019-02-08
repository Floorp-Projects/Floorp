const EXPECTED_CHAIN_SUBJECT_CNs = [
  "CN=include-subdomains.pinning.example.com",
  "CN=Alternate Trusted Authority"
  ];
const EXPECTED_CHAIN_ISSUER_CN = "CN=Alternate Trusted Authority";

const MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE = -16384;

function parseReport(request) {
  // read the report from the request
  let inputStream = Components.classes["@mozilla.org/scriptableinputstream;1"].createInstance(Components.interfaces.nsIScriptableInputStream);
  inputStream.init(request.bodyInputStream, 0x01, 0004, 0);

  let body = "";
  if (inputStream) {
    while (inputStream.available()) {
      body = body + inputStream.read(inputStream.available());
    }
  }
  // parse the report
  return JSON.parse(body);
}

function handleRequest(request, response) {
  let report = {};
  let certChain = [];

  switch (request.queryString) {
    case "succeed":
      report = parseReport(request);
      certChain = report.failedCertChain;

      // ensure the cert chain is what we expect
      if (certChain.length != EXPECTED_CHAIN_SUBJECT_CNs.length) {
        response.setStatusLine("1.1", 500, "Server error");
        response.setHeader("X-Error", "chain wrong size");
        response.write("<html>The report contained an unexpectedly sized chain</html>");
        return;
      }

      let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(Ci.nsIX509CertDB);
      for (let idx in certChain) {
        let cert = certdb.constructX509FromBase64(certChain[idx]);
        if (cert.issuerName != EXPECTED_CHAIN_ISSUER_CN) {
          response.setStatusLine("1.1", 500, "Server error");
          response.write("<html>The report contained a certificate with an unexpected issuer</html>");
          return;
        }
        if (cert.subjectName != EXPECTED_CHAIN_SUBJECT_CNs[idx]) {
          response.setStatusLine("1.1", 500, "Server error");
          response.write("<html>The report contained a certificate with an unexpected subject</html>");
          return;
        }
      }

      if (report.errorCode !== MOZILLA_PKIX_ERROR_KEY_PINNING_FAILURE) {
        response.setStatusLine("1.1", 500, "Server error");
        response.write("<html>The report contained an unexpected error code</html>");
        return;
      }

      // if all is as expected, send the 201 the client expects
      response.setStatusLine("1.1", 201, "Created");
      response.write("<html>OK</html>");
      break;
    case "nocert":
      report = parseReport(request);
      certChain = report.failedCertChain;

      if (certChain && certChain.length > 0) {
        // We're not expecting a chain; if there is one, send an error
        response.setStatusLine("1.1", 500, "Server error");
        response.write("<html>The report contained an unexpected chain</html>");
        return;
      }

      // if all is as expected, send the 201 the client expects
      response.setStatusLine("1.1", 201, "Created");
      response.write("<html>OK</html>");
      break;
    case "badcert":
      report = parseReport(request);
      certChain = report.failedCertChain;

      if (!certChain || certChain.length != 2) {
        response.setStatusLine("1.1", 500, "Server error");
        response.write("<html>The report contained an unexpected chain</html>");
        return;
      }

      // if all is as expected, send the 201 the client expects
      response.setStatusLine("1.1", 201, "Created");
      response.write("<html>OK</html>");
      break;
    case "error":
      response.setStatusLine("1.1", 500, "Server error");
      response.write("<html>server error</html>");
      break;
    default:
      response.setStatusLine("1.1", 500, "Server error");
      response.write("<html>succeed, nocert or error expected (got " + request.queryString + ")</html>");
      break;
  }
}
