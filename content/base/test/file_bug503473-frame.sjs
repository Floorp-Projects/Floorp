function handleRequest(request, response) {
  response.processAsync();
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  response.setHeader("Cache-Control", "no-cache", false);

  response.write(
    '<!DOCTYPE html>' +
    '<div></div>' +
    '<script>' +
    'function doWrite() {' +
    '  document.write("<p></p>");' +
    '  parent.done();' +
    '  document.close();' +
    '}' +
    'setTimeout(doWrite, 0);' +
    '</script>' 
  );

  response.bodyOutputStream.flush();
  // leave the stream open
}

