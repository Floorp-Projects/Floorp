function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 307, "Moved temporarly");
  response.setHeader("Location", "http://mochi.test:8888/tests/dom/tests/mochitest/ajax/offline/updating.cacheManifest");
  response.setHeader("Content-Type", "text/cache-manifest");
}
