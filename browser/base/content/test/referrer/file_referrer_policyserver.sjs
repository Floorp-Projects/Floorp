/**
 * Renders a link with the provided referrer policy.
 * Used in browser_referrer_*.js, bug 1113431.
 * Arguments: ?scheme=http://&policy=origin&rel=noreferrer
 */
function handleRequest(request, response) {
  let query = new URLSearchParams(request.queryString);

  let scheme = query.get("scheme");
  let policy = query.get("policy");
  let rel = query.get("rel");
  let cross = query.get("cross");

  let host = cross ? "example.com" : "test1.example.com";
  let linkUrl =
    scheme +
    host +
    "/browser/browser/base/content/test/referrer/" +
    "file_referrer_testserver.sjs";
  let metaReferrerTag = policy
    ? `<meta name='referrer' content='${policy}'>`
    : "";

  let html = `<!DOCTYPE HTML>
              <html>
              <head>
              <meta charset='utf-8'>
              ${metaReferrerTag}
              <title>Test referrer</title>
              </head>
              <body>
              <a id='testlink' href='${linkUrl}' ${rel ? ` rel='${rel}'` : ""}>
              referrer test link</a>
              </body>
              </html>`;

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.write(html);
}
