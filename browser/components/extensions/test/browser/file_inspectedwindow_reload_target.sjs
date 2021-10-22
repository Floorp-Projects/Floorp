/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80 ft=javascript: */
"use strict";

Components.utils.importGlobalProperties(["URLSearchParams"]);

function handleRequest(request, response) {
  let params = new URLSearchParams(request.queryString);

  switch (params.get("test")) {
    case "cache":
      handleCacheTestRequest(request, response);
      break;

    case "user-agent":
      handleUserAgentTestRequest(request, response);
      break;

    case "injected-script":
      handleInjectedScriptTestRequest(request, response, params);
      break;
  }
}

function handleCacheTestRequest(request, response) {
  response.setHeader("Content-Type", "text/plain; charset=UTF-8", false);

  if (request.hasHeader("pragma") && request.hasHeader("cache-control")) {
    response.write(
      `${request.getHeader("pragma")}:${request.getHeader("cache-control")}`
    );
  } else {
    response.write("empty cache headers");
  }
}

function handleUserAgentTestRequest(request, response) {
  response.setHeader("Content-Type", "text/html", false);

  const userAgentHeader = request.hasHeader("user-agent")
    ? request.getHeader("user-agent")
    : null;

  const query = new URLSearchParams(request.queryString);
  if (query.get("crossOriginIsolated") === "true") {
    response.setHeader("Cross-Origin-Opener-Policy", "same-origin", false);
  }

  const IFRAME_HTML = `
  <!doctype html>
  <html>
    <head>
      <meta charset=utf8>
      <script>
        globalThis.initialUserAgent = navigator.userAgent;
      </script>
    </head>
    <body>
      <h1>Iframe</h1>
    </body>
  </html>`;
  // We always want the iframe to have a different host from the top-level document.
  const iframeHost =
    request.host === "example.com" ? "example.org" : "example.com";
  const iframeOrigin = `${request.scheme}://${iframeHost}`;
  const iframeUrl = `${iframeOrigin}/document-builder.sjs?html=${encodeURI(
    IFRAME_HTML
  )}`;

  const HTML = `
  <!doctype html>
  <html>
    <head>
      <meta charset=utf8>
      <title>test</title>
      <script>
        "use strict";
        /*
         * Store the user agent very early in the document loading process
         * so we can assert in tests that it is set early enough.
         */
        globalThis.initialUserAgent = navigator.userAgent;
        globalThis.userAgentHeader = ${JSON.stringify(userAgentHeader)};
      </script>
    </head>
    <body>
      <h1>Top-level</h1>
      <h2>${userAgentHeader ?? "no user-agent header"}</h2>
      <iframe src='${iframeUrl}'></iframe>
    </body>
  </html>`;

  response.write(HTML);
}

function handleInjectedScriptTestRequest(request, response, params) {
  response.setHeader("Content-Type", "text/html; charset=UTF-8", false);

  let content = "";
  const frames = parseInt(params.get("frames"));
  if (frames > 0) {
    // Output an iframe in seamless mode, so that there is an higher chance that in case
    // of test failures we get a screenshot where the nested iframes are all visible.
    content = `<iframe seamless src="?test=injected-script&frames=${frames -
      1}"></iframe>`;
  }

  response.write(`<!DOCTYPE html>
    <html>
      <head>
       <meta charset="utf-8">
       <style>
         iframe { width: 100%; height: ${frames * 150}px; }
       </style>
      </head>
      <body>
       <h1>IFRAME ${frames}</h1>
       <pre>injected script NOT executed</pre>
       <script type="text/javascript">
         window.pageScriptExecutedFirst = true;
       </script>
       ${content}
      </body>
    </html>
  `);
}
