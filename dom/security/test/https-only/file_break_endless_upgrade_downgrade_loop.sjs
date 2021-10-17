// Custom *.sjs file specifically for the needs of Bug 1691888
"use strict";

const REDIRECT_META = `
  <html>
  <head>
    <meta http-equiv="refresh" content="0; url='http://example.com/tests/dom/security/test/https-only/file_break_endless_upgrade_downgrade_loop.sjs?test1b'">
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
       let url= "http://example.com/tests/dom/security/test/https-only/file_break_endless_upgrade_downgrade_loop.sjs?test2b";
       window.location = url;
     </script>
   </body>
   </html>`;

const REDIRECT_302 =
  "http://example.com/tests/dom/security/test/https-only/file_break_endless_upgrade_downgrade_loop.sjs?test3b";

const REDIRECT_302_DIFFERENT_PATH =
  "http://example.com/tests/dom/security/test/https-only/file_user_gesture.html";

function handleRequest(request, response) {
  // avoid confusing cache behaviour
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  // if the scheme is not https, meaning that the initial request did not
  // get upgraded, then we rather fall through and display unexpected content.
  if (request.scheme === "https") {
    let query = request.queryString;
   
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

    if (query === "test4a") {
      response.setStatusLine("1.1", 302, "Found");
      response.setHeader("Location", REDIRECT_302_DIFFERENT_PATH, false);
      return;
    }
  }

  // we should never get here, just in case,
  // let's return something unexpected
  response.write("<html><body>DO NOT DISPLAY THIS</body></html>")
}
