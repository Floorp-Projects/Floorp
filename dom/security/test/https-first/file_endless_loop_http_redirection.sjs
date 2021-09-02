const RELAOD_HTTP = `
<html class="no-js">
  <head>
    <title>HTTPS not supported - Bureau of Meteorology</title>
    <script language="Javascript">
      var home_page = 'http://example.com/tests/dom/security/test/https-first/file_endless_loop_http_redirection.sjs' ;
      window.location = home_page;
    </script>
</hmtl>
`;
const RESPONSE_SUCCESS = `
  <html>
    <body>
      send message, downgraded
    <script type="application/javascript">
      window.opener.postMessage({result: 'downgraded', scheme: 'http'}, '*');
    </script>
    </body>
  </html>`;

const REDIRECT_307 =
  "http://example.com/tests/dom/security/test/https-first/file_endless_loop_http_redirection.sjs?start";

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  // Every https request gets redirected
  if (request.scheme === "https") {
    response.setStatusLine("1.1", 307, "Temporary Redirect");
    response.setHeader("Location", REDIRECT_307, true);
    return;
  }
  // If a 307 redirection took place redirect to same site without query
  if (request.queryString === "start") {
    response.write(RELAOD_HTTP);
    return;
  }
  // we should get here
  response.write(RESPONSE_SUCCESS);
}
