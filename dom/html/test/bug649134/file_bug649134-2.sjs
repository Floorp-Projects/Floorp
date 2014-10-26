function handleRequest(request, response)
{
  response.seizePower();
  var r = 'HTTP/1.1 200 OK\r\n' +
          'Content-Type: text/html\r\n' +
          'Link: <		\014>; rel="stylesheet",\r\n' +
          '\r\n' +
          '<!-- selector {} body {display:none;} --><body>PASS</body>\r\n';
  response.bodyOutputStream.write(r, r.length);
  response.bodyOutputStream.flush();
  response.finish();
}
