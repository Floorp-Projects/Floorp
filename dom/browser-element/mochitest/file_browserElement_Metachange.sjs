function handleRequest(request, response)
{
  var p = request.queryString.split('|');
  response.setHeader('Content-Language', p[0], false);
  response.write('<html><head><meta name="application-name" content="sjs"' +
                  (p.length > 1 ? (' lang="' + p[1] + '"') : '') + '></head><body></body></html>');
}
