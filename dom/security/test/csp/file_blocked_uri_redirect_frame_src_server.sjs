// Redirect server specifically for the needs of Bug 1687342

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);

  let query = request.queryString;
  if (query === "doredirect") {
    var newLocation = "http://test1.example.com/tests/dom/security/test/csp/file_blocked_uri_redirect_frame_src_server.sjs?query#ref2";
    response.setStatusLine("1.1", 302, "Found");
    response.setHeader("Location", newLocation, false);
    return;
  }
}
