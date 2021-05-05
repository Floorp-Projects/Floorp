// Custom *.sjs file specifically for the needs of Bug 1707856
"use strict";

const REDIRECT_META = `
  <html>
  <head>
    <meta http-equiv="refresh" content="0; url='http://example.com/tests/dom/security/test/https-first/file_redirect_downgrade.sjs?testEnd'">
  </head>
  <body>
    META REDIRECT
  </body>
  </html>`;

const REDIRECT_JS = `
  <html>
   <body>
     JS REDIRECT
     <script>
       let url= "http://example.com/tests/dom/security/test/https-first/file_redirect_downgrade.sjs?testEnd";
       window.location = url;
     </script>
   </body>
   </html>`;

const REDIRECT_302 =
  "http://example.com/tests/dom/security/test/https-first/file_redirect_downgrade.sjs?testEnd";

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
      window.opener.postMessage({result: 'error', scheme: scheme}, '*');
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
      response.write(REDIRECT_META);
      return;
    }

    if (query === "test2a") {
      response.write(REDIRECT_JS);
      return;
    }

    if (query === "test3a") {
      response.setStatusLine("1.1", 302, "Found");
      response.setHeader("Location", REDIRECT_302, false);
      return;
    }

    // Simulating a timeout by processing the https request
    response.processAsync();
    return;
  }

  // We should arrive here when the redirection was downraded successful
  if (query == "testEnd") {
    response.write(RESPONSE_SUCCESS);
    return;
  }
  // We should never arrive here, just in case send 'error'
  response.write(RESPONSE_UNEXPECTED);
}
