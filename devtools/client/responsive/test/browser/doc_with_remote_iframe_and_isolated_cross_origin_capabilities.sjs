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
        globalThis.initialOrientationAngle = screen.orientation.angle;
        globalThis.initialOrientationType = screen.orientation.type;
      </script>
    </head>
    <body>
      <h1>Iframe</h1>
    </body>
  </html>`;

  const HTML = `
  <!doctype html>
  <html>
    <head>
      <script>
        globalThis.initialOrientationAngle = screen.orientation.angle;
        globalThis.initialOrientationType = screen.orientation.type;
      </script>
      <meta charset=utf8>
    </head>
    <body>
      <h1>Top-level document</h1>
      <iframe src='${iframeOrigin}/document-builder.sjs?html=${encodeURI(
    IFRAME_HTML
  )}'></iframe>
    </body>
  </html>`;

  response.write(HTML);
}
