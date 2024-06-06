"use strict";

// DOWNGRADE_REDIRECT_*: http instead of https, otherwise same path
const DOWNGRADE_REDIRECT_META = `
  <html>
  <head>
    <meta http-equiv="refresh" content="0; url='http://example.com/tests/dom/security/test/https-first/file_break_endless_upgrade_downgrade_loop.sjs?downgrade_redirect_meta'">
  </head>
  <body>
    META REDIRECT
  </body>
  </html>`;

const DOWNGRADE_REDIRECT_JS = `
  <html>
   <body>
     JS REDIRECT
     <script>
       let url= "http://example.com/tests/dom/security/test/https-first/file_break_endless_upgrade_downgrade_loop.sjs?downgrade_redirect_js";
       window.location = url;
     </script>
   </body>
   </html>`;

// REDIRECT_*: different path and http instead of https
const REDIRECT_META = `
  <html>
  <head>
    <meta http-equiv="refresh" content="0; url='http://example.com/tests/dom/security/test/https-first/file_downgrade_with_different_path.sjs?redirect_meta'">
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
       let url= "http://example.com/tests/dom/security/test/https-first/file_downgrade_with_different_path.sjs?redirect_js";
       window.location = url;
     </script>
   </body>
   </html>`;

// An onload postmessage to window opener
const RESPONSE_HTTP_SCHEME = `
  <html>
  <body>
  <script type="application/javascript">
    window.opener.postMessage({result: 'scheme-http-'+window.location}, '*');
  </script>
  </body>
  </html>`;

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.scheme == "https") {
    // allow http status code as parameter
    const query = request.queryString.split("=");
    if (query[0] == "downgrade_redirect_http") {
      let location = `http://${request.host}${request.path}?${request.queryString}`;
      response.setStatusLine(request.httpVersion, query[1], "Found");
      response.setHeader("Location", location, false);
    } else if (query[0] == "redirect_http") {
      response.setStatusLine(request.httpVersion, query[1], "Found");
      let location =
        "http://example.com/tests/dom/security/test/https-first/file_downgrade_with_different_path.sjs?" +
        request.queryString;
      response.setHeader("Location", location, false);
    } else if (query[0] == "downgrade_redirect_js") {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.write(DOWNGRADE_REDIRECT_JS);
    } else if (query[0] == "redirect_js") {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.write(REDIRECT_JS);
    } else if (query[0] == "downgrade_redirect_meta") {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.write(DOWNGRADE_REDIRECT_META);
    } else if (query[0] == "redirect_meta") {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.write(REDIRECT_META);
    } else {
      // We should never get here, but just in case ...
      response.setStatusLine(request.httpVersion, 500, "OK");
      response.write("unexepcted query");
    }
    return;
  }

  // return http response
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.write(RESPONSE_HTTP_SCHEME);
}
