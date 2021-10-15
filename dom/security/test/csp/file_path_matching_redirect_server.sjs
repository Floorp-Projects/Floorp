// Redirect server specifically to handle redirects
// for path-level host-source matching
// see https://bugzilla.mozilla.org/show_bug.cgi?id=808292

function handleRequest(request, response)
{

  var newLocation = "http://test1.example.com/tests/dom/security/test/csp/file_path_matching.js";

  response.setStatusLine("1.1", 302, "Found");
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Location", newLocation, false);
}
