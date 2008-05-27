function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/cache-manifest");
  response.setHeader("Cache-Control", "no-cache");

  response.write("CACHE MANIFEST\n");
  response.write("#" + Date.now() + "\n");
  response.write("http://localhost:8888/tests/dom/tests/mochitest/ajax/offline/changing1Hour.sjs\n");
  response.write("http://localhost:8888/tests/dom/tests/mochitest/ajax/offline/changing1Sec.sjs\n");
}

