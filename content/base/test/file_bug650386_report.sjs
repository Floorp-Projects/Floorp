// SJS file for tests for bug650386, this serves as CSP violation report target
// and issues a redirect, to make sure the browser does not post to the target
// of the redirect, per CSP spec.
// This handles 301, 302, 303 and 307 redirects. The HTTP status code
// returned/type of redirect to do comes from the query string
// parameter
function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);

  var redirect = request.queryString;

  var loc = "http://example.com/some/fake/path";
  response.setStatusLine("1.1", redirect, "Found");
  response.setHeader("Location", loc, false);
  return;
}
