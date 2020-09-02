// Bug 1658264 - HTTPS-Only and iFrames
// see browser_iframe_test.js

const IFRAME_CONTENT = `
<!DOCTYPE HTML>
<html>
  <head><meta charset="utf-8"></head>
  <body>Helo Friend!</body>
</html>`;
const DOCUMENT_CONTENT = q => `
<!DOCTYPE HTML>
<html>
  <head><meta charset="utf-8"></head>
  <body>
    <iframe src="http://example.com/browser/dom/security/test/https-only/file_iframe_test.sjs?com-${q}"></iframe>
    <iframe src="http://example.org/browser/dom/security/test/https-only/file_iframe_test.sjs?org-${q}"></iframe>
  </body>
</html>`;

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);
  let queryString = request.queryString;
  const queryScheme = request.scheme;

  // Setup the state with an empty string and return "ok"
  if (queryString == "setup") {
    setState("receivedQueries", "");
    response.write("ok");
    return;
  }

  let receivedQueries = getState("receivedQueries");

  // Return result-string
  if (queryString == "results") {
    response.write(receivedQueries);
    return;
  }

  // Add semicolon to seperate strings
  if (receivedQueries !== "") {
    receivedQueries += ";";
  }

  // Requests from iFrames start with com or org
  if (queryString.startsWith("com-") || queryString.startsWith("org-")) {
    receivedQueries += queryString;
    setState("receivedQueries", `${receivedQueries}-${queryScheme}`);
    response.write(IFRAME_CONTENT);
    return;
  }

  // Everything else has to be a top-level request
  receivedQueries += `top-${queryString}`;
  setState("receivedQueries", `${receivedQueries}-${queryScheme}`);
  response.write(DOCUMENT_CONTENT(queryString));
}
