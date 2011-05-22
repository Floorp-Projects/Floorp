function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 307, "Moved temporarily");
  response.setHeader("Location", "http://example.org/tests/dom/plugins/test/loremipsum.txt");
  response.setHeader("Content-Type", "text/html");
}
