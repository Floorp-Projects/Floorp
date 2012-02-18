function handleRequest(request, response)
{
  response.setHeader('Content-Type', 'text/plain', false);
  if (request.hasHeader('Referer')) {
    response.write('Referer found: ' + request.getHeader('Referer'));
  }
  else {
    response.write('No Referer found');
  }
}
