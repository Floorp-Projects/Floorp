function handleRequest(request, response)
{
  // Echos the referrer back to the requester.
  response.setHeader('Content-Type', 'text/plain', false);
  response.write(request.getHeader('Referer'));
}
