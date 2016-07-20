// custom *.sjs for Bug 1273430
// META CSP: upgrade-insecure-requests

// important: the IFRAME_URL is *http* and needs to be upgraded to *https* by upgrade-insecure-requests
const IFRAME_URL =
  "http://example.com/tests/dom/security/test/csp/file_upgrade_insecure_docwrite_iframe.sjs?docwriteframe";

const TEST_FRAME = `
  <!DOCTYPE HTML>
  <html><head><meta charset="utf-8">
  <title>TEST_FRAME</title>
  <meta http-equiv="Content-Security-Policy" content="upgrade-insecure-requests">
  </head>
  <body>
  <script type="text/javascript">
    document.write('<iframe src="` + IFRAME_URL + `"/>');
  </script>
  </body>
  </html>`;


// doc.write(iframe) sends a post message to the parent indicating the current
// location so the parent can make sure the request was upgraded to *https*.
const DOC_WRITE_FRAME = `
  <!DOCTYPE HTML>
  <html><head><meta charset="utf-8">
  <title>DOC_WRITE_FRAME</title>
  </head>
  <body onload="window.parent.parent.postMessage({result: document.location.href}, '*');">
  </body>
  </html>`;

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);

  var queryString = request.queryString;

  if (queryString === "testframe") {
    response.write(TEST_FRAME);
    return;
  }

  if (queryString === "docwriteframe") {
    response.write(DOC_WRITE_FRAME);
    return;
  }

  // we should never get here, but just in case
  // return something unexpected
  response.write("do'h");
}
