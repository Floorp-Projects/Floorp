function handleRequest(request, response)
{
  response.setStatusLine(null, 302, "Moved");
  response.setHeader("Location", "http://nosuchdomain.localhost", false);
}

