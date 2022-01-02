const RESPONSE_SUCCESS = `
  <html>
    <body>
      send message, downgraded
    <script type="application/javascript">
      let scheme = document.location.protocol;
      window.opener.postMessage({result: 'downgraded', scheme: scheme}, '*');
    </script>
    </body>
  </html>`;

const RESPONSE_UNEXPECTED = `
  <html>
    <body>
      send message, error
    <script type="application/javascript">
      let scheme = document.location.protocol;
      window.opener.postMessage({result: 'Error', scheme: scheme}, '*');
    </script>
    </body>
  </html>`;

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  // if the received request is not http send an error
  if (request.scheme === "http") {
    response.write(RESPONSE_SUCCESS);
    return;
  }
  // we should never get here; just in case, return something unexpected
  response.write(RESPONSE_UNEXPECTED);
}
