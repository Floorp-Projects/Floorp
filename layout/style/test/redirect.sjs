function handleRequest(request, response)
{
  dump("redirect.sjs: handling request with query " + request.queryString + "\n");
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location", request.queryString, false);
}
