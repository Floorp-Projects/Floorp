"use strict";

const TESTFRAME_QUERY_LARGE_ALLOCATION_WITH_NO_CSP =
 `<!DOCTYPE HTML>
  <html>
  <head>
    <title>Bug 1555050 - Test CSP Navigation using ReloadInFreshProcess</title>
  </head>
  <body>
    testframe<br/>
    <script>
      // we have to use noopener otherwise the window is not the only window in the tabgroup which is needed
      // for "Large-Allocation" to succeed
      window.open("http://test1.example.com/tests/dom/security/test/csp/file_reloadInFreshProcess.sjs?largeAllocation_with_no_csp", "_blank", "noopener");
    </script>
  </body>
  </html>`;

const TESTFRAME_QUERY_LARGE_ALLOCATION_WITH_CSP =
 `<!DOCTYPE HTML>
  <html>
  <head>
    <title>Bug 1555050 - Test CSP Navigation using ReloadInFreshProcess</title>
  </head>
  <body>
    testframe<br/>
    <script>
      // we have to use noopener otherwise the window is not the only window in the tabgroup which is needed
      // for "Large-Allocation" to succeed
      window.open("http://test2.example.com/tests/dom/security/test/csp/file_reloadInFreshProcess.sjs?largeAllocation_with_csp", "_blank", "noopener");
    </script>
  </body>
  </html>`;

const LARGE_ALLOCATION = 
 `<!DOCTYPE HTML>
  <html>
  <head>
    <title>Bug 1555050 - Test CSP Navigation using ReloadInFreshProcess</title>
  </head>
  <body>
    largeAllocation<br/>
    <script>
      window.close();
    </script>
  </body>
  </html>`;

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
 
  var queryString = request.queryString;

  if (queryString == "testframe_with_csp") {
    response.setHeader("Content-Security-Policy", "upgrade-insecure-requests", false);     
    response.write(TESTFRAME_QUERY_LARGE_ALLOCATION_WITH_NO_CSP);
    return;
  }

  if (queryString == "testframe_with_no_csp") {
    response.write(TESTFRAME_QUERY_LARGE_ALLOCATION_WITH_CSP);
    return;
  }

  if (queryString == "largeAllocation_with_csp") {
    response.setHeader("Content-Security-Policy", "upgrade-insecure-requests", false);   
    response.setHeader("Large-Allocation", "0", false);
    response.write(LARGE_ALLOCATION);
    return;
  }

  if (queryString == "largeAllocation_with_no_csp") {
    response.setHeader("Large-Allocation", "0", false);
    response.write(LARGE_ALLOCATION);
    return;
  }

  // we should never get here, but just in case return something unexpected
  response.write("do'h");
}
