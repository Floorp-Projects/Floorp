function handleRequest(request, response)
{
  function etag(count) {
    return '"anetag' + count + '"';
  }

  var count = parseInt(getState('count'));
  if (!count)
    count = 0;

  // reload(false) will make a request with If-None-Match headers
  var ifNoneMatch = request.hasHeader("If-None-Match") ?
    request.getHeader("If-None-Match") : "";

  if (ifNoneMatch === etag(count)) {
    response.setStatusLine(request.httpVersion, "304", "Not Modified");
    return;
  }

  count++;
  setState('count', count + '');

  response.setHeader('Content-Type', 'text/html', false);
  response.setHeader('Cache-Control', 'public, max-age=3600', false);
  response.setHeader("ETag", etag(count), false);
  response.write('<html><body>' + count + '</body></html>');
}
