function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/plain", false);
  response.write(request.queryString);
}

