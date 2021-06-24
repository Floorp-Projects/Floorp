const RESPONSE_ERROR = `
  <html>
  <body>
  Error occurred...
  <script type="application/javascript">
    window.opener.postMessage({result: 'ERROR'}, '*');
  </script>
  </body>
  </html>`;
const RESPONSE_POLICY = `
<html>
<body>
Send policy onload...
<script type="application/javascript">
  const loc = document.location.href;
  window.opener.postMessage({result: document.referrer, location: loc}, "*");
</script>
</body>
</html>`;

const expectedQueries = [
  "no-referrer",
  "no-referrer-when-downgrade",
  "origin",
  "origin-when-cross-origin",
  "same-origin",
  "strict-origin",
  "strict-origin-when-cross-origin",
  "unsafe-url",
];

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  // eslint-disable-next-line mozilla/reject-importGlobalProperties
  Cu.importGlobalProperties(["URLSearchParams"]);
  let query = new URLSearchParams(request.queryString);
  if (query.has("sendMe")) {
    response.write(RESPONSE_POLICY);
    return;
  }
  // Get the referrer policy that we want to set
  let referrerPolicy = query.get("rp");
  //If the query contained one of the expected referrer policies send a request with the given policy,
  // else send error
  if (expectedQueries.includes(referrerPolicy)) {
    // Downgrade to http if upgrade equals 0
    if (query.get("upgrade") === "0" && request.scheme === "https") {
      // Simulating a timeout by processing the https request
      response.processAsync();
      return;
    }
    // create js redirection that request with the given (related to the query) referrer policy
    const SEND_REQUEST = `
      <html>
      <head>
        <meta name="referrer" content=${referrerPolicy}>
      </head>
      <body>
      JS REDIRECT
      <script>
        let url = 'https://example.com/tests/dom/security/test/https-first/file_referrer_policy.sjs?sendMe';
        window.location = url;
      </script>
      </body>
      </html>`;
    response.write(SEND_REQUEST);
    return;
  }

  // We should never get here but in case we send an error
  response.setStatusLine(request.httpVersion, 500, "OK");
  response.write(RESPONSE_ERROR);
}
