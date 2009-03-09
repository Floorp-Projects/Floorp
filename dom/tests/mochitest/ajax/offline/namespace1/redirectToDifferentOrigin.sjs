function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 307, "Moved temporarly");
  response.setHeader("Location", "http://example.org/tests/dom/tests/mochitest/ajax/offline/fallback2.html");
  response.setHeader("Content-Type", "text/html");
}
