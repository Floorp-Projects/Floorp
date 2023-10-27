// Redirect server specifically for the needs of Bug 1542194

"use strict";

let REDIRECT_302_URI =
  "http://test1.example.com/tests/dom/security/test/csp/file_blocked_uri_in_violation_event_after_redirects.sjs?test1b#ref1b";

let JS_REDIRECT = `<html>
   <body>
   <script>
     var url= "http://test2.example.com/tests/dom/security/test/csp/file_blocked_uri_in_violation_event_after_redirects.sjs?test2b#ref2b";
     window.location = url;
   </script>
   </body>
   </html>`;

let LINK_CLICK_NAVIGATION = `<html>
   <body>
   <a id="navlink" href="http://test3.example.com/tests/dom/security/test/csp/file_blocked_uri_in_violation_event_after_redirects.sjs?test3b#ref3b">click me</a>
   <script>
     window.onload = function() { document.getElementById('navlink').click(); }
   </script>
   </body>
   </html>`;

function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  let query = request.queryString;

  // Test 1: 302 redirect
  if (query === "test1a") {
    var newLocation = REDIRECT_302_URI;
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", newLocation, false);
    return;
  }

  // Test 2: JS redirect
  if (query === "test2a") {
    response.setHeader("Content-Type", "text/html", false);
    response.write(JS_REDIRECT);
    return;
  }

  // Test 3: Link navigation
  if (query === "test3a") {
    response.setHeader("Content-Type", "text/html", false);
    response.write(LINK_CLICK_NAVIGATION);
  }
}
