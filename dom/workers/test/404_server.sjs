function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 404, "Not found");

  // Any valid JS.
  if (request.queryString == 'js') {
    response.setHeader("Content-Type", "text/javascript", false);
    response.write('4 + 4');
  }
}
