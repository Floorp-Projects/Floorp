// Custom *.sjs file specifically for the needs of Bug 1709552
"use strict";

const RESPONSE_SUCCESS = `
  <html>
    <body>
      send message, downgraded
    <script type="application/javascript">
      let scheme = document.location.protocol;
      window.opener.postMessage({result: 'downgraded', scheme: scheme}, '*');
    </script>
    </body>
  </html>`;

const RESPONSE_UNEXPECTED = `
  <html>
    <body>
      send message, error
    <script type="application/javascript">
      let scheme = document.location.protocol;
      window.opener.postMessage({result: 'Error', scheme: scheme}, '*');
    </script>
    </body>
  </html>`;

function handleRequest(request, response) {
  // avoid confusing cache behaviour
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  let query = request.queryString;
  // if the scheme is not https and it is the initial request
  // then we rather fall through and display unexpected content
  if (request.scheme === "https") {
    if (query === "test1a") {
      response.setStatusLine("1.1", 501, "Not Implemented");
      response.write("Not Implemented\n");
      return;
    }

    if (query === "test2a") {
      response.setStatusLine("1.1", 504, "Gateway Timeout");
      response.write("Gateway Timeout\n");
      return;
    }

    if (query === "test3a") {
      response.setStatusLine("1.1", 521, "Web Server Is Down");
      response.write("Web Server Is Down\n");
      return;
    }
    if (query === "test4a") {
      response.setStatusLine("1.1", 530, "Railgun Error");
      response.write("Railgun Error\n");
      return;
    }
    if (query === "test5a") {
      response.setStatusLine("1.1", 560, "Unauthorized");
      response.write("Unauthorized\n");
      return;
    }

    // We should never arrive here, just in case send something unexpected
    response.write(RESPONSE_UNEXPECTED);
    return;
  }

  // We should arrive here when the redirection was downraded successful
  response.write(RESPONSE_SUCCESS);
}
