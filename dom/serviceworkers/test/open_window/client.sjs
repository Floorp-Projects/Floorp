/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const RESPONSE = `
<!DOCTYPE HTML>
<html>
<head>
  <title>Bug 1172870 - page opened by ServiceWorkerClients.OpenWindow</title>
</head>
<body>
<p id="display"></p>
<div id="content" style="display: none"></div>
<pre id="test"></pre>
<h1>client.sjs</h1>
<script class="testbody" type="text/javascript">

  window.onload = function() {
    if (document.domain === "mochi.test") {
      navigator.serviceWorker.ready.then(function(result) {
        navigator.serviceWorker.onmessage = function(event) {
          if (event.data !== "CLOSE") {
            dump("ERROR: unexepected reply from the service worker.\\n");
          }
          if (parent) {
            parent.postMessage("CLOSE", "*");
          }
          window.close();
        }

        let message = window.crossOriginIsolated ? "NEW_ISOLATED_WINDOW" : "NEW_WINDOW";
        navigator.serviceWorker.controller.postMessage(message);
      })
    } else {
      window.onmessage = function(event) {
        if (event.data !== "CLOSE") {
            dump("ERROR: unexepected reply from the iframe.\\n");
        }
        window.close();
      }

      var iframe = document.createElement('iframe');
      iframe.src = "http://mochi.test:8888/tests/dom/serviceworkers/test/open_window/client.sjs";
      document.body.appendChild(iframe);
    }
  }

</script>
</pre>
</body>
</html>
`;

function handleRequest(request, response) {
  let query = new URLSearchParams(request.queryString);

  // If the request has been marked to be isolated with COOP+COEP, set the appropriate headers.
  if (query.get("crossOriginIsolated") == "true") {
    response.setHeader("Cross-Origin-Opener-Policy", "same-origin", false);
  }

  // Always set the COEP and CORP headers, so that this document can be framed
  // by a document which has also set COEP to require-corp.
  response.setHeader("Cross-Origin-Embedder-Policy", "require-corp", false);
  response.setHeader("Cross-Origin-Resource-Policy", "cross-origin", false);

  response.setHeader("Content-Type", "text/html", false);
  response.write(RESPONSE);
}
