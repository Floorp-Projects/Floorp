// Custom *.sjs file specifically for the needs of Bug 1453814

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
  "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");

const FRAME = `
  <!DOCTYPE html>
  <html>
  <head>
    <title>Bug 1453814 - Do not allow same-site cookies for cross origin redirect</title>
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

const SAME_ORIGIN = "http://mochi.test:8888/"
const CROSS_ORIGIN = "http://example.com/";
const PATH = "tests/dom/security/test/general/file_same_site_cookies_redirect.sjs";

const FRAME_META_REFRESH_SAME = `
  <html><head>
  <meta http-equiv="refresh" content="0;
   url='` + SAME_ORIGIN + PATH + `?loadFrame'">
  </head></html>`;

const FRAME_META_REFRESH_CROSS = `
  <html><head>
  <meta http-equiv="refresh" content="0;
   url='` + CROSS_ORIGIN + PATH + `?loadFrame'">
  </head></html>`;

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.queryString === "setSameSiteCookie") {
    response.setHeader("Set-Cookie", "myKey=strictSameSiteCookie; samesite=strict", true);
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  if (request.queryString === "sameToSameRedirect") {
    let URL = SAME_ORIGIN + PATH + "?loadFrame";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", URL, false);
    return;
  }

  if (request.queryString === "sameToCrossRedirect") {
    let URL = CROSS_ORIGIN + PATH + "?loadFrame";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", URL, false);
    return;
  }

  if (request.queryString === "crossToSameRedirect") {
    let URL = SAME_ORIGIN + PATH + "?loadFrame";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", URL, false);
    return;
  }

  if (request.queryString === "sameToCrossRedirectMeta") {
    response.write(FRAME_META_REFRESH_CROSS);
    return;
  }

  if (request.queryString === "crossToSameRedirectMeta") {
    response.write(FRAME_META_REFRESH_SAME);
    return;
  }

  if (request.queryString === "loadFrame") {
    response.write(FRAME);
    return;
  }

  // we should never get here, but just in case return something unexpected
  response.write("D'oh");
}
