// Custom *.sjs file specifically for the needs of Bug 1452496

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

const FRAME = `
  <!DOCTYPE html>
  <html>
  <head>
    <title>Bug 1452496 - Do not allow same-site cookies in cross site context</title>
  </head>
  <body>
    <script type="application/javascript">
      let cookie = document.cookie;
      // now reset the cookie for the next test
      document.cookie = "myKey=;" + "expires=Thu, 01 Jan 1970 00:00:00 GMT";
      window.parent.postMessage({result: cookie}, 'http://mochi.test:8888');
    </script>
  </body>
  </html>`;

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.queryString.includes("setSameSiteCookie")) {
    response.setHeader(
      "Set-Cookie",
      "myKey=strictSameSiteCookie; samesite=strict",
      true
    );
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  if (request.queryString.includes("setRegularCookie")) {
    response.setHeader("Set-Cookie", "myKey=regularCookie;", true);
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  if (request.queryString.includes("loadFrame")) {
    response.write(FRAME);
    return;
  }

  // we should never get here, but just in case return something unexpected
  response.write("D'oh");
}
