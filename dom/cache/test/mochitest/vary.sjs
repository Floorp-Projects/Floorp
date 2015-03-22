function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Vary", request.getHeader("WhatToVary"));
  response.write(request.getHeader("WhatToVary"));
}
