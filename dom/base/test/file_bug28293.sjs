function handleRequest(request, response)
{
  response.setHeader("Content-Type", "text/plain", false);
  response.write(decodeURIComponent(request.queryString));
}
