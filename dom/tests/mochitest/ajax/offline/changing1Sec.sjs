function handleRequest(request, response)
{
  response.setStatusLine(request.httpVersion, 200, "Ok");
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "max-age=1");

  response.write(Date.now());
}

