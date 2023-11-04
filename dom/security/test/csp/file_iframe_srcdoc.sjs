// Custom *.sjs file specifically for the needs of
// https://bugzilla.mozilla.org/show_bug.cgi?id=1073952

"use strict";

const SCRIPT = `
  <script>
    parent.parent.postMessage({result: &quot;allowed&quot;}, &quot;*&quot;);
  </script>`;

const SIMPLE_IFRAME_SRCDOC =
  `
  <!DOCTYPE html>
  <html>
  <head><meta charset="utf-8"></head>
  <body>
    <iframe sandbox="allow-scripts" srcdoc="` +
  SCRIPT +
  `"></iframe>
  </body>
  </html>`;

const INNER_SRCDOC_IFRAME = `
  <iframe sandbox='allow-scripts' srcdoc='<script>
      parent.parent.parent.postMessage({result: &quot;allowed&quot;}, &quot;*&quot;);
    </script>'>
  </iframe>`;

const NESTED_IFRAME_SRCDOC =
  `
  <!DOCTYPE html>
  <html>
  <head><meta charset="utf-8"></head>
  <body>
    <iframe sandbox="allow-scripts" srcdoc="` +
  INNER_SRCDOC_IFRAME +
  `"></iframe>
  </body>
  </html>`;

const INNER_DATAURI_IFRAME = `
  <iframe sandbox='allow-scripts' src='data:text/html,<script>
      parent.parent.parent.postMessage({result: &quot;allowed&quot;}, &quot;*&quot;);
    </script>'>
  </iframe>`;

const NESTED_IFRAME_SRCDOC_DATAURI =
  `
  <!DOCTYPE html>
  <html>
  <head><meta charset="utf-8"></head>
  <body>
    <iframe sandbox="allow-scripts" srcdoc="` +
  INNER_DATAURI_IFRAME +
  `"></iframe>
  </body>
  </html>`;

function handleRequest(request, response) {
  const query = new URLSearchParams(request.queryString);

  response.setHeader("Cache-Control", "no-cache", false);
  if (typeof query.get("csp") === "string") {
    response.setHeader("Content-Security-Policy", query.get("csp"), false);
  }
  response.setHeader("Content-Type", "text/html", false);

  if (query.get("action") === "simple_iframe_srcdoc") {
    response.write(SIMPLE_IFRAME_SRCDOC);
    return;
  }

  if (query.get("action") === "nested_iframe_srcdoc") {
    response.write(NESTED_IFRAME_SRCDOC);
    return;
  }

  if (query.get("action") === "nested_iframe_srcdoc_datauri") {
    response.write(NESTED_IFRAME_SRCDOC_DATAURI);
    return;
  }

  // we should never get here, but just in case
  // return something unexpected
  response.write("do'h");
}
