const CC = Components.Constructor;
const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

const RESPONSE_SUCCESS = `
  <html>
    <body>
      send message, downgraded
    <script type="application/javascript">
      let scheme = document.location.protocol;
      const loc = document.location.href;
      window.opener.postMessage({location: loc, scheme: scheme, form:"test=success" }, '*');
    </script>
    </body>
  </html>`;

const POST_FORMULAR = `
<html>
    <body>
        <form action="http://example.com/tests/dom/security/test/https-first/file_form_submission.sjs?" method="POST" id="POSTForm">
            <div>
             <label id="submit">Submit</label>
             <input name="test" id="form" value="success">
            </div>
        </form>
        <script class="testbody" type="text/javascript">
            document.getElementById("POSTForm").submit();
        </script>
    </body>
</html>
`;

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  let queryString = request.queryString;
  if (request.scheme === "https" && queryString === "test=1") {
    response.write(RESPONSE_SUCCESS);
    return;
  }
  if (
    request.scheme === "https" &&
    (queryString === "test=2" || queryString === "test=4")
  ) {
    // time out request
    response.processAsync();
    return;
  }
  if (request.scheme === "http" && queryString === "test=2") {
    response.write(RESPONSE_SUCCESS);
    return;
  }
  if (queryString === "test=3" || queryString === "test=4") {
    // send post form
    response.write(POST_FORMULAR);
    return;
  }
  if (request.method == "POST") {
    // extract form parameters
    let body = new BinaryInputStream(request.bodyInputStream);
    let avail;
    let bytes = [];
    while ((avail = body.available()) > 0) {
      Array.prototype.push.apply(bytes, body.readByteArray(avail));
    }
    let requestBodyContents = String.fromCharCode.apply(null, bytes);

    response.write(`
    <html>
    <script type="application/javascript">
      let scheme = document.location.protocol;
      const loc = document.location.href;
      window.opener.postMessage({location: loc, scheme: scheme, form: '${requestBodyContents}'}, '*');
    </script>
  </html>`);

    return;
  }
  // we should never get here; just in case, return something unexpected
  response.write("do'h");
}
