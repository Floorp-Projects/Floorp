function handleRequest(request, response)
{
  response.processAsync();
  response.setHeader("Content-Type", "application/x-Second-Test", false);

  response.write("Hello world.\n");
  response.finish();
}
