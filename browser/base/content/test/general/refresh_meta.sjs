/**
 * Will cause an auto-refresh to the URL provided in the query string
 * after some delay using a <meta> tag.
 *
 * Expects the query string to be in the format:
 *
 * ?p=[URL of the page to redirect to]&d=[delay]
 *
 * Example:
 *
 * ?p=http%3A%2F%2Fexample.org%2Fbrowser%2Fbrowser%2Fbase%2Fcontent%2Ftest%2Fgeneral%2Frefresh_meta.sjs&d=200
 */
function handleRequest(request, response) {
  let query = new URLSearchParams(request.queryString);

  let page = query.get("p");
  let delay = query.get("d");

  let html = `<!DOCTYPE HTML>
              <html>
              <head>
              <meta charset='utf-8'>
              <META http-equiv='refresh' content='${delay}; url=${page}'>
              <title>Gonna refresh you, folks.</title>
              </head>
              <body>
              <h1>Wait for it...</h1>
              </body>
              </html>`;

  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(request.httpVersion, "200", "Found");
  response.setHeader("Cache-Control", "no-cache", false);
  response.write(html);
}
