function handleRequest(request, response) {
  var key = '/.well-known/idp-proxy/' + request.queryString;
  dump(getState(key) + '\n');
  if (request.method === 'GET') {
    if (getState(key)) {
      response.setStatusLine(request.httpVersion, 200, 'OK');
    } else {
      response.setStatusLine(request.httpVersion, 404, 'Not Found');
    }
  } else if (request.method === 'PUT') {
    setState(key, 'OK');
    response.setStatusLine(request.httpVersion, 200, 'OK');
  } else {
    response.setStatusLine(request.httpVersion, 406, 'Method Not Allowed');
  }
  response.setHeader('Content-Type', 'text/plain;charset=UTF-8');
  response.write('OK');
}
