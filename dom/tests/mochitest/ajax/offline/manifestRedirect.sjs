function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 307, "Moved temporarly");
  response.setHeader("Location", "https://example.com/tests/dom/tests/mochitest/ajax/offline/updating.cacheManifest");
  response.setHeader("Content-Type", "text/cache-manifest");
}
