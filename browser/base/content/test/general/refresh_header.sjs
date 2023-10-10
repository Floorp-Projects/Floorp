/**
 * Will cause an auto-refresh to the URL provided in the query string
 * after some delay using the refresh HTTP header.
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

  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(request.httpVersion, "200", "Found");
  response.setHeader("refresh", `${delay}; url=${page}`);
  response.write("OK");
}
