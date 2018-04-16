// Custom *.sjs file specifically for the needs of Bug 1454242

const WIN = `
  <html>
  <body>
  <script type="application/javascript">
    let newWin = window.open("http://mochi.test:8888/tests/dom/security/test/general/file_same_site_cookies_toplevel_set_cookie.sjs?loadWinAndSetCookie");
    newWin.onload = function() {
      newWin.close();
    }
  </script>
  </body>
  </html>`;

const DUMMY_WIN = `
  <html>
  <body>
  just a dummy window that sets a same-site=lax cookie
  <script type="application/javascript">
    window.opener.opener.postMessage({value: 'testSetupComplete'}, '*');
  </script>
  </body>
  </html>`;

const FRAME = `
  <html>
  <body>
  <script type="application/javascript">
    let cookie = document.cookie;
    // now reset the cookie for the next test
    document.cookie = "myKey=;" + "expires=Thu, 01 Jan 1970 00:00:00 GMT";
    window.parent.postMessage({value: cookie}, 'http://mochi.test:8888');
  </script>
  </body>
  </html>`;

const SAME_ORIGIN = "http://mochi.test:8888/"
const CROSS_ORIGIN = "http://example.com/";
const PATH = "tests/dom/security/test/general/file_same_site_cookies_redirect.sjs";

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.queryString === "loadWin") {
    response.write(WIN);
    return;
  }

  if (request.queryString === "loadWinAndSetCookie") {
    response.setHeader("Set-Cookie", "myKey=laxSameSiteCookie; samesite=lax", true);
    response.write(DUMMY_WIN);
    return;
  }

  if (request.queryString === "checkCookie") {
    response.write(FRAME);
    return;
  }

  // we should never get here, but just in case return something unexpected
  response.write("D'oh");
}
