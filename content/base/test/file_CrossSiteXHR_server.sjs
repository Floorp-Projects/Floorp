function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  var isPreflight = request.method == "OPTIONS";

  // Check that request was correct

  if (!isPreflight && "headers" in query) {
    headers = eval(query.headers);
    for(headerName in headers) {
      if (request.getHeader(headerName) != headers[headerName]) {
        sendHttp500(response,
          "Header " + headerName + " had wrong value. Expected " +
          headers[headerName] + " got " + request.getHeader(headerName));
        return;
      }
    }
  }

  if (isPreflight && "requestHeaders" in query &&
      request.getHeader("Access-Control-Request-Headers") != query.requestHeaders) {
    sendHttp500(response,
      "Access-Control-Request-Headers had wrong value. Expected " +
      query.requestHeaders + " got " +
      request.getHeader("Access-Control-Request-Headers"));
    return;
  }

  if (isPreflight && "requestMethod" in query &&
      request.getHeader("Access-Control-Request-Method") != query.requestMethod) {
    sendHttp500(response,
      "Access-Control-Request-Method had wrong value. Expected " +
      query.requestMethod + " got " +
      request.getHeader("Access-Control-Request-Method"));
    return;
  }

  if ("origin" in query && request.getHeader("Origin") != query.origin) {
    sendHttp500(response,
      "Origin had wrong value. Expected " + query.origin + " got " +
      request.getHeader("Origin"));
    return;
  }

  if ("cookie" in query) {
    cookies = {};
    request.getHeader("Cookie").split(/ *; */).forEach(function (val) {
      [name, value] = val.split('=');
      cookies[name] = unescape(value);
    });
    
    query.cookie.split(",").forEach(function (val) {
      [name, value] = val.split('=');
      if (cookies[name] != value) {
        sendHttp500(response,
          "Cookie " + name  + " had wrong value. Expected " + value +
          " got " + cookies[name]);
        return;
      }
    });
  }

  if ("noCookie" in query && request.hasHeader("Cookie")) {
    sendHttp500(response,
      "Got cookies when didn't expect to: " + request.getHeader("Cookie"));
    return;
  }

  // Send response
       
  if (query.allowOrigin && (!isPreflight || !query.noAllowPreflight))
    response.setHeader("Access-Control-Allow-Origin", query.allowOrigin);

  if (query.allowCred)
    response.setHeader("Access-Control-Allow-Credentials", "true");

  if (query.setCookie)
    response.setHeader("Set-Cookie", query.setCookie + "; path=/");

  if (isPreflight) {
    if (query.allowHeaders)
      response.setHeader("Access-Control-Allow-Headers", query.allowHeaders);

    if (query.allowMethods)
      response.setHeader("Access-Control-Allow-Methods", query.allowMethods);

    return;
  }

  response.setHeader("Content-Type", "application/xml", false);
  response.write("<res>hello pass</res>\n");
}

function sendHttp500(response, text) {
  response.setStatusLine(null, 500, text);
}
