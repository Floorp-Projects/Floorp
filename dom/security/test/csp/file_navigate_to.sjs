// Custom *.sjs file specifically for the needs of
// https://bugzilla.mozilla.org/show_bug.cgi?id=1529068

"use strict";

const TEST_NAVIGATION_HEAD = `
  <!DOCTYPE HTML>
  <html>
  <head>
    <title>Bug 1529068 Implement CSP 'navigate-to' directive</title>`;

const TEST_NAVIGATION_AFTER_META = `
  </head>
  <body>
    <script type="text/javascript">
    window.location = "`;

const TEST_NAVIGATION_FOOT = `";
    </script>
  </body>
  </html>
  `;

function handleRequest(request, response) {
  const query = new URLSearchParams(request.queryString);

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  if (query.get("redir")) {
    response.setStatusLine(request.httpVersion, "302", "Found");
    response.setHeader("Location", query.get("redir"), false);
    return;
  }

  response.write(TEST_NAVIGATION_HEAD);

  // We need meta to set multiple CSP headers.
  if (query.get("csp")) {
    response.write(
      '<meta http-equiv="Content-Security-Policy" content="' +
        query.get("csp") +
        '">'
    );
  }
  if (query.get("csp2")) {
    response.write(
      '<meta http-equiv="Content-Security-Policy" content="' +
        query.get("csp2") +
        '">'
    );
  }

  response.write(
    TEST_NAVIGATION_AFTER_META + query.get("target") + TEST_NAVIGATION_FOOT
  );
}
