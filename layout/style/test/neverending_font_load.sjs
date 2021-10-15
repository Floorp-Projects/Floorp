function handleRequest(request, response)
{
  response.processAsync();
  response.setHeader("Content-Type", "application/octet-stream", false);
  response.write("");
}
