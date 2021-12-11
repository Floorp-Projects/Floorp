// Custom *.sjs file specifically for the needs of Bug 1706126
"use strict";
// subdomain of example.com
const REDIRECT_302 =
  "http://www.redirect-example.com/tests/dom/security/test/https-first/file_downgrade_request_upgrade_request.sjs";

const RESPONSE_SUCCESS = `
  <html>
    <body>
      send message, upgraded
    <script type="application/javascript">
      let scheme = document.location.protocol;
      window.opener.postMessage({result: 'upgraded', scheme: scheme}, '*');
    </script>
    </body>
  </html>`;

const RESPONSE_UNEXPECTED = `
  <html>
    <body>
      send message, error
    <script type="application/javascript">
      let scheme = document.location.protocol;
      window.opener.postMessage({result: 'error', scheme: scheme}, '*');
    </script>
    </body>
  </html>`;

function handleRequest(request, response) {
  // avoid confusing cache behaviour
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  // if the scheme is https and it is the initial request time it out
  if (request.scheme === "https" && request.host === "redirect-example.com") {
    // Simulating a timeout by processing the https request
    response.processAsync();
    return;
  }
  if (request.scheme === "http" && request.host === "redirect-example.com") {
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", REDIRECT_302, false);
    return;
  }
  // if the request was sent to subdomain
  if (request.host.startsWith("www.")) {
    response.write(RESPONSE_SUCCESS);
    return;
  }
  // We should never arrive here, just in case send 'error'
  response.write(RESPONSE_UNEXPECTED);
}
