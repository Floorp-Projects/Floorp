function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/event-stream", false);
  response.write("data: eventsource response from server!");
  response.write("\n\n");
}
