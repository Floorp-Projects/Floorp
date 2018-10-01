function handleRequest(request, response)
{
  var count = parseInt(getState('count'));
  if (!count || request.queryString == 'countreset')
    count = 0;

  setState('count', count + 1 + '');

  response.setHeader('Content-Type', 'text/html', false);
  response.setHeader('Cache-Control', 'max-age=0');
  response.write('<html><body onload="opener.onChildLoad()" ' +
                 'onunload="parseInt(\'0\')">' +
                 count + '</body></html>');
}
