function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location", request.queryString, false);
}
