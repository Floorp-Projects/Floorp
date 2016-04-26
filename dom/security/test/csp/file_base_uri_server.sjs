// Custom *.sjs file specifically for the needs of
// https://bugzilla.mozilla.org/show_bug.cgi?id=1263286

"use strict";
Components.utils.importGlobalProperties(["URLSearchParams"]);

const PRE_BASE = `
  <!DOCTYPE HTML>
  <html>
  <head>
  <title>Bug 1045897 - Test CSP base-uri directive</title>`;

const REGULAR_POST_BASE =`
  </head>
  <body onload='window.parent.postMessage({result: document.baseURI}, "*");'>
  <!-- just making use of the 'base' tag for this test -->
  </body>
  </html>`;

const SCRIPT_POST_BASE = `
  </head>
  <body>
  <script>
    document.getElementById("base1").removeAttribute("href");
    window.parent.postMessage({result: document.baseURI}, "*");
  </script>
  </body>
  </html>`;

function handleRequest(request, response) {
  const query = new URLSearchParams(request.queryString);

  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // Deliver the CSP policy encoded in the URL
  response.setHeader("Content-Security-Policy", query.get("csp"), false);

  // Send HTML to test allowed/blocked behaviors
  response.setHeader("Content-Type", "text/html", false);
  response.write(PRE_BASE);
  var base1 =
    "<base id=\"base1\" href=\"" + query.get("base1") + "\">";
  var base2 =
    "<base id=\"base2\" href=\"" + query.get("base2") + "\">";
  response.write(base1 + base2);

  if (query.get("action") === "enforce-csp") {
    response.write(REGULAR_POST_BASE);
    return;
  }

  if (query.get("action") === "remove-base1") {
    response.write(SCRIPT_POST_BASE);
    return;
  }

  // we should never get here, but just in case
  // return something unexpected
  response.write("do'h");
}
