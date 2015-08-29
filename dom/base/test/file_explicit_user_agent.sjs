function handleRequest(request, response)
{
  if (request.hasHeader("User-Agent")) {
    response.setHeader("Result-User-Agent",
                       request.getHeader("User-Agent"));
  }
  response.write("");
}
