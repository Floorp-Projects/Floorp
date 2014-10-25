function handleRequest(request, response) {
  let invalidHeaders = ["Cookie"];
  let headers = {};

  if (request.queryString == "expectAuth=true") {
    if (request.hasHeader("Authorization")) {
      headers["authorization"] = request.getHeader("Authorization");
    } else {
      response.setStatusLine(null, 401, "Authentication required");
      response.setHeader("WWW-Authenticate", "basic realm=\"testrealm\"", true);
    }
  } else {
    invalidHeaders.push("Authorization");
  }

  for each (let header in invalidHeaders) {
    if (request.hasHeader(header)) {
      response.setStatusLine(null, 500, "Server Error");
      headers[header.toLowerCase()] = request.getHeader(header);
    }
  }

  response.write(JSON.stringify(headers));
}
