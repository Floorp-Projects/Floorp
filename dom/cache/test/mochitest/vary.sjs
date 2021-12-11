function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  var header = "no WhatToVary header";
  if (request.hasHeader("WhatToVary")) {
    header = request.getHeader("WhatToVary");
    response.setHeader("Vary", header);
  }
  response.write(header);
}
