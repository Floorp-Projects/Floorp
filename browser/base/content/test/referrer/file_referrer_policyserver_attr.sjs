/**
 * Renders a link with the provided referrer policy.
 * Used in browser_referrer_*.js, bug 1113431.
 * Arguments: ?scheme=http://&policy=origin&rel=noreferrer
 */
function handleRequest(request, response)
{
  Components.utils.importGlobalProperties(["URLSearchParams"]);
  let query = new URLSearchParams(request.queryString);

  let scheme = query.get("scheme");
  let policy = query.get("policy");
  let rel = query.get("rel");

  let linkUrl = scheme +
                "test1.example.com/browser/browser/base/content/test/referrer/" +
                "file_referrer_testserver.sjs";
  let referrerPolicy =
      policy ? `referrerpolicy="${policy}"` : "";

  let html = `<!DOCTYPE HTML>
              <html>
              <head>
              <meta charset='utf-8'>
              <title>Test referrer</title>
              </head>
              <body>
              <a id='testlink' href='${linkUrl}' ${referrerPolicy} ${rel ? ` rel='${rel}'` : ""}>
              referrer test link</a>
              </body>
              </html>`;

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.write(html);
}
