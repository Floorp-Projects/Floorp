// Custom *.sjs file specifically for the needs of Bug 1286861

// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
  "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");

const FRAME = `
  <!DOCTYPE html>
  <html>
  <head>
    <title>Bug 1286861 - Add support for same site cookies</title>
  </head>
  <body>
    <script type="application/javascript">
      let myWin = window.open("http://mochi.test:8888/tests/dom/security/test/general/file_same_site_cookies_toplevel_nav.sjs?loadWin");
      myWin.onload = function() {
        myWin.close();
      }
    </script>
  </body>
  </html>`;

const WIN = `
  <!DOCTYPE html>
  <html>
  <body>
    just a dummy window
  </body>
  </html>`;

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.queryString.includes("setStrictSameSiteCookie")) {
    response.setHeader("Set-Cookie", "myKey=strictSameSiteCookie; samesite=strict", true);
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  if (request.queryString.includes("setLaxSameSiteCookie")) {
    response.setHeader("Set-Cookie", "myKey=laxSameSiteCookie; samesite=lax", true);
    response.setHeader("Content-Type", "image/png");
    response.write(IMG_BYTES);
    return;
  }

  // save the object state of the initial request, which returns
  // async once the server has processed the img request.
  if (request.queryString.includes("queryresult")) {
    response.processAsync();
    setObjectState("queryResult", response);
    return;
  }

  if (request.queryString.includes("loadFrame")) {
    response.write(FRAME);
    return;
  }

  if (request.queryString.includes("loadWin")) {
    var cookie = "unitialized";
    if (request.hasHeader("Cookie")) {
      cookie = request.getHeader("Cookie");
    }
    else {
      cookie = "myKey=noCookie";
    }
    response.write(WIN);

    // return the result
    getObjectState("queryResult", function(queryResponse) {
      if (!queryResponse) {
        return;
      }
      queryResponse.write(cookie);
      queryResponse.finish();
    });
    return;
  }

  // we should never get here, but just in case return something unexpected
  response.write("D'oh");
}
