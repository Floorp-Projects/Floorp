function handleRequest(request, response)
{
  try {
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  var isPreflight = request.method == "OPTIONS";

  // Check that request was correct
  if (!isPreflight && "headers" in query) {
    headers = eval(query.headers);
    for(headerName in headers) {
      if (request.getHeader(headerName) != headers[headerName]) {
        throw "Header " + headerName + " had wrong value. Expected " +
              headers[headerName] + " got " + request.getHeader(headerName);
      }
    }
  }
  if (isPreflight && "requestHeaders" in query &&
      request.getHeader("Access-Control-Request-Headers") != query.requestHeaders) {
    throw "Access-Control-Request-Headers had wrong value. Expected " +
          query.requestHeaders + " got " +
          request.getHeader("Access-Control-Request-Headers");
  }
  if (isPreflight && "requestMethod" in query &&
      request.getHeader("Access-Control-Request-Method") != query.requestMethod) {
    throw "Access-Control-Request-Method had wrong value. Expected " +
          query.requestMethod + " got " +
          request.getHeader("Access-Control-Request-Method");
  }
  if ("origin" in query && request.getHeader("Origin") != query.origin) {
    throw "Origin had wrong value. Expected " + query.origin + " got " +
          request.getHeader("Origin");
  }


  // Send response

  if (query.allowOrigin && (!isPreflight || !query.noAllowPreflight))
    response.setHeader("Access-Control-Allow-Origin", query.allowOrigin);


  if (isPreflight) {
    if (query.allowHeaders)
      response.setHeader("Access-Control-Allow-Headers", query.allowHeaders);

    if (query.allowMethods)
      response.setHeader("Access-Control-Allow-Methods", query.allowMethods);

    return;
  }

  response.setHeader("Content-Type", "application/xml", false);
  response.write("<res>hello pass</res>\n");
  
  } catch (e) {
    dump(e + "\n");
    throw e;
  }
}
