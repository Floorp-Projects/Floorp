const SITE_META_REDIRECT = `
<html>
  <head>
    <meta http-equiv="refresh" content="0; url='https://example.com/tests/dom/security/test/sec-fetch/file_redirect.sjs?redirect302'">
  </head>
  <body>
    META REDIRECT
  </body>
</html>
`;

const REDIRECT_302 =
  "https://example.com/tests/dom/security/test/sec-fetch/file_redirect.sjs?pageC";

function handleRequest(req, res) {
  // avoid confusing cache behaviour
  res.setHeader("Cache-Control", "no-cache", false);
  res.setHeader("Content-Type", "text/html", false);

  switch (req.queryString) {
    case "meta":
      res.write(SITE_META_REDIRECT);
      return;
    case "redirect302":
      res.setStatusLine("1.1", 302, "Found");
      res.setHeader("Location", REDIRECT_302, false);
      return;
    case "pageC":
      res.write("<html><body>PAGE C</body></html>");
      return;
  }

  res.write(`<html><body>THIS SHOULD NEVER BE DISPLAYED</body></html>`);
}
