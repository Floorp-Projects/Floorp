"use strict";

// An onload postmessage to window opener
const RESPONSE_HTTPS_SCHEME = `
  <html>
  <body>
  <script type="application/javascript">
    window.opener.postMessage({result: 'scheme-https'}, '*');
  </script>
  </body>
  </html>`;

const RESPONSE_HTTP_SCHEME = `
  <html>
  <body>
  <script type="application/javascript">
    window.opener.postMessage({result: 'scheme-http'}, '*');
  </script>
  </body>
  </html>`;

function handleRequest(request, response) {
  let response_content = request.scheme === "https"
    ? RESPONSE_HTTPS_SCHEME
    : RESPONSE_HTTP_SCHEME;
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(response_content);
  return;
}
