function handleRequest(request, response)
{
  response.processAsync();
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("X-Frame-Options", "DENY", false);
  
  response.finish();
}
