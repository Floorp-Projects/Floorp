// Custom *.sjs file specifically for the needs of Bug 1452496

const SET_COOKIE_FRAME = `
  <!DOCTYPE html>
  <html>
  <head>
    <title>Bug 1452496 - Do not allow same-site cookies in cross site context</title>
  </head>
  <body>
    <script type="application/javascript">
      document.cookie = "myKey=sameSiteCookieInlineScript;SameSite=strict";
    </script>
  </body>
  </html>`;

const GET_COOKIE_FRAME = `
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

function handleRequest(request, response)
{
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  if (request.queryString.includes("setSameSiteCookieUsingInlineScript")) {
    response.write(SET_COOKIE_FRAME);
    return;
  }

  if (request.queryString.includes("getCookieFrame")) {
    response.write(GET_COOKIE_FRAME);
    return;
  }

  // we should never get here, but just in case return something unexpected
  response.write("D'oh");
}
