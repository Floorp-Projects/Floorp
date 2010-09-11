function handleRequest(request, response)
{
  // Redirect to another domain.
  // Using 307 to keep the method.
  response.setStatusLine(null, 307, "Temp");
  response.setHeader("Location",
                     "http://example.org:80/tests/content/html/content/test/583288_submit_server.sjs");
}
