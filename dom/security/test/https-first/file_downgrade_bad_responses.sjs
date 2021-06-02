// Custom *.sjs file specifically for the needs of Bug 1709552 
"use strict";

const RESPONSE_SUCCESS = `
  <html>
    <body>
      send message, downgraded
    <script type="application/javascript">
      window.opener.postMessage({result: 'downgraded', scheme: 'http'}, '*');
    </script>
    </body>
  </html>`;

const RESPONSE_UNEXPECTED = `
  <html>
    <body>
      send message, error
    <script type="application/javascript">
      window.opener.postMessage({result: 'Error', scheme: 'http'}, '*');
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
      response.setStatusLine("1.1", 400, "Bad Request");
      response.write("Bad Request\n");
      return;
    }

    if (query === "test2a") {
      response.setStatusLine("1.1", 403, "Forbidden");
      response.write("Forbidden\n");
      return;
    }

    if (query === "test3a") {
      response.setStatusLine("1.1", 404, "Not Found");
      response.write("Not Found\n");
      return;
    }
    if (query === "test4a") {
      response.setStatusLine("1.1", 416, "Requested Range Not Satisfiable");
      response.write("Requested Range Not Satisfiable\n");
      return;
    }
    if (query === "test5a") {
      response.setStatusLine("1.1", 418, "I'm a teapot");
      response.write("I'm a teapot\n");
      return;
    }
    if (query == "test6a") {
      // Simulating a timeout by processing the https request
      response.processAsync();
      return;
    }

    // We should never arrive here, just in case send something unexpected
    response.write(RESPONSE_UNEXPECTED);
    return;
  }

  // We should arrive here when the redirection was downraded successful
  response.write(RESPONSE_SUCCESS);
}
