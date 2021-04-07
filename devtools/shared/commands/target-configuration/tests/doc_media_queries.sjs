"use strict";

function handleRequest(request, response) {
  response.setHeader("Content-Type", "html", false);

  // Check the params and set the cross-origin-opener policy headers if needed
  Components.utils.importGlobalProperties(["URLSearchParams"]);
  const query = new URLSearchParams(request.queryString);
  if (query.get("crossOriginIsolated") === "true") {
    response.setHeader("Cross-Origin-Opener-Policy", "same-origin", false);
  }

  // We always want the iframe to have a different host from the top-level document.
  const iframeHost =
    request.host === "example.com" ? "example.org" : "example.com";
  const iframeOrigin = `${request.scheme}://${iframeHost}`;

  const IFRAME_HTML = `
    <!doctype html>
  <html>
    <head>
      <meta charset=utf8>
      <script>
        globalThis.initialMatchesPrefersDarkColorScheme =
          window.matchMedia("(prefers-color-scheme: dark)").matches;
      </script>
      <style>
        html { background: cyan;}
        @media (prefers-color-scheme: dark) {
          html {background: darkred;}
        }
      </style>
    </head>
    <body>Iframe</body>
  </html>`;

  const HTML = `
  <!doctype html>
  <html>
    <head>
      <meta charset=utf8>
      <title>test</title>
      <script type="application/javascript">
        "use strict";

        /* Store the result of dark color-scheme match very early in the document loading process
         * so we can assert in tests that the simulation starts early enough.
         */
        globalThis.initialMatchesPrefersDarkColorScheme =
          window.matchMedia("(prefers-color-scheme: dark)").matches;
      </script>
      <style>
        @media (prefers-color-scheme: dark) {
        html {
            background-color: darkblue;
          }
        }
      </style>
    </head>
    <body>
      <h1>Test color-scheme simulation</h1>
      <iframe src='${iframeOrigin}/document-builder.sjs?html=${encodeURI(
    IFRAME_HTML
  )}'></iframe>
    </body>
  </html>`;

  response.write(HTML);
}
