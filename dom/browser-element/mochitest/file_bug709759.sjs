function handleRequest(request, response)
{
  response.processAsync();
  response.setHeader("Content-Type", "image/jpeg", false);
}