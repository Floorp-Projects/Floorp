/**
 * This file expects a query string that's the upper-cased version of a file to
 * be redirected to in the same directory.  The redirect will also include
 * added "secret data" as a query string.
 *
 * So if the request is `/path/redirect_with_query_args.sjs?FOO.JS` the redirect
 * will be to `/path/foo.js?SECRET_DATA`.
 **/

function handleRequest(request, response) {
  // The secret data to include in the redirect to make the redirect URL
  // easily detectable.
  const secretData = "SECRET_DATA";

  let pathBase = request.path.split("/").slice(0, -1).join("/");
  let targetFile = request.queryString.toLowerCase();
  let newUrl = `${pathBase}/${targetFile}?${secretData}`;

  response.setStatusLine(request.httpVersion, 302, "Found");
  response.setHeader("Cache-Control", "no-cache");
  response.setHeader("Location", newUrl, false);
}
