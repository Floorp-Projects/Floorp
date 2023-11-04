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
function readQuery(testCase) {
  let twoValues = testCase.split("-");
  let upgradeRequest = twoValues[0] === "https" ? 1 : 0;
  let httpsResponse = twoValues[1] === "https" ? 1 : 0;
  return [upgradeRequest, httpsResponse];
}

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  let query = new URLSearchParams(request.queryString);
  // Downgrade to test http/https -> HTTP referrer policy
  if (query.has("sendMe2") && request.scheme === "https") {
    // Simulating a timeout by processing the https request
    response.processAsync();
    return;
  }
  if (query.has("sendMe") || query.has("sendMe2")) {
    response.write(RESPONSE_POLICY);
    return;
  }
  // Get the referrer policy that we want to set
  let referrerPolicy = query.get("rp");
  //If the query contained one of the expected referrer policies send a request with the given policy,
  // else send error
  if (expectedQueries.includes(referrerPolicy)) {
    // Determine the test case, e.g. don't upgrade request but send response in https
    let testCase = readQuery(query.get("upgrade"));
    let httpsRequest = testCase[0];
    let httpsResponse = testCase[1];
    // Downgrade to http if upgrade equals 0
    if (httpsRequest === 0 && request.scheme === "https") {
      // Simulating a timeout by processing the https request
      response.processAsync();
      return;
    }
    // create js redirection that request with the given (related to the query) referrer policy
    const SEND_REQUEST_HTTPS = `
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
    const SEND_REQUEST_HTTP = `
      <html>
      <head>
        <meta name="referrer" content=${referrerPolicy}>
      </head>
      <body>
      JS REDIRECT
      <script>
        let url = 'http://example.com/tests/dom/security/test/https-first/file_referrer_policy.sjs?sendMe2';
        window.location = url;
      </script>
      </body>
      </html>`;
    let respond = httpsResponse === 1 ? SEND_REQUEST_HTTPS : SEND_REQUEST_HTTP;
    response.write(respond);
    return;
  }

  // We should never get here but in case we send an error
  response.setStatusLine(request.httpVersion, 500, "OK");
  response.write(RESPONSE_ERROR);
}
