// Custom *.sjs file specifically for the needs of Bug:
// Bug 1048048 - CSP violation report not sent for @import

const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  var queryString = request.queryString;

  // (1) lets process the queryresult request async and
  // wait till we have received the image request.
  if (queryString === "queryresult") {
    response.processAsync();
    setObjectState("queryResult", response);
    return;
  }

  // (2) handle the csp-report and return the JSON back to
  // the testfile using the afore stored xml request in (1).
  if (queryString === "report") {
    getObjectState("queryResult", function(queryResponse) {
      if (!queryResponse) {
        return;
      }

      // send the report back to the XML request for verification
      var report = new BinaryInputStream(request.bodyInputStream);
      var avail;
      var bytes = [];
      while ((avail = report.available()) > 0) {
        Array.prototype.push.apply(bytes, report.readByteArray(avail));
      }
      var data = String.fromCharCode.apply(null, bytes);
      queryResponse.bodyOutputStream.write(data, data.length);
      queryResponse.finish();
    });
    return;
  }

  // we should not get here ever, but just in case return
  // something unexpected.
  response.write("doh!");
}
