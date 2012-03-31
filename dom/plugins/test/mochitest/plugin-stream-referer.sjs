function handleRequest(request, response)
{
  response.setHeader('Content-Type', 'text/plain', false);
  response.setHeader('Cache-Control', 'no-cache', false);
  response.setHeader('Content-Type', 'application/x-test', false);
  if (request.hasHeader('Referer')) {
    response.write('Referer found: ' + request.getHeader('Referer'));
  }
  else {
    response.write('No Referer found');
  }
}
