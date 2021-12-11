// https://bugzilla.mozilla.org/show_bug.cgi?id=1702001

// An onload postmessage to window opener
const ON_LOAD = `
  <html>
  <body>
  send onload message...
  <script type="application/javascript">
    window.opener.postMessage({result: 'you entered the http page'}, '*');
  </script>
  </body>
  </html>`;

// When an https request is sent, cause a timeout so that the https-only error
// page is displayed.
function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  if (request.scheme === "https") {
    // Simulating a timeout by processing the https request
    // async and *never* return anything!
    response.processAsync();
    return;
  }
  if (request.scheme === "http") {
    response.write(ON_LOAD);
  }
}
