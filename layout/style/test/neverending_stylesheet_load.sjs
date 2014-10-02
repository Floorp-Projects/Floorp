function handleRequest(request, response)
{
  response.processAsync();
  response.setHeader("Content-Type", "text/css", false);
  response.write("");
}
