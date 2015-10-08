const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

function handleRequest(request, response)
{
  var query = {};
  request.queryString.split('&').forEach(function (val) {
    var [name, value] = val.split('=');
    query[name] = unescape(value);
  });

  var isPreflight = request.method == "OPTIONS";

  var bodyStream = new BinaryInputStream(request.bodyInputStream);
  var bodyBytes = [];
  while ((bodyAvail = bodyStream.available()) > 0)
    Array.prototype.push.apply(bodyBytes, bodyStream.readByteArray(bodyAvail));

  var body = decodeURIComponent(
    escape(String.fromCharCode.apply(null, bodyBytes)));

  if (query.hop) {
    query.hop = parseInt(query.hop, 10);
    hops = eval(query.hops);
    var curHop = hops[query.hop - 1];
    query.allowOrigin = curHop.allowOrigin;
    query.allowHeaders = curHop.allowHeaders;
    query.allowCred = curHop.allowCred;
    if (curHop.setCookie) {
      query.setCookie = unescape(curHop.setCookie);
    }
    if (curHop.cookie) {
      query.cookie = unescape(curHop.cookie);
    }
    query.noCookie = curHop.noCookie;
  }

  // Check that request was correct

  if (!isPreflight && query.body && body != query.body) {
    sendHttp500(response, "Wrong body. Expected " + query.body + " got " +
      body);
    return;
  }

  if (!isPreflight && "headers" in query) {
    headers = eval(query.headers);
    for(headerName in headers) {
      // Content-Type is changed if there was a body 
      if (!(headerName == "Content-Type" && body) &&
          request.getHeader(headerName) != headers[headerName]) {
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
      var [name, value] = val.split('=');
      cookies[name] = unescape(value);
    });
    
    query.cookie.split(",").forEach(function (val) {
      var [name, value] = val.split('=');
      if (cookies[name] != value) {
        sendHttp500(response,
          "Cookie " + name  + " had wrong value. Expected " + value +
          " got " + cookies[name]);
        return;
      }
    });
  }

  if (query.noCookie && request.hasHeader("Cookie")) {
    sendHttp500(response,
      "Got cookies when didn't expect to: " + request.getHeader("Cookie"));
    return;
  }

  // Send response

  if (!isPreflight && query.status) {
    response.setStatusLine(null, query.status, query.statusMessage);
  }
  if (isPreflight && query.preflightStatus) {
    response.setStatusLine(null, query.preflightStatus, "preflight status");
  }
  
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
  }
  else {
    if (query.responseHeaders) {
      let responseHeaders = eval(query.responseHeaders);
      for (let responseHeader in responseHeaders) {
        response.setHeader(responseHeader, responseHeaders[responseHeader]);
      }
    }

    if (query.exposeHeaders)
      response.setHeader("Access-Control-Expose-Headers", query.exposeHeaders);
  }

  if (query.hop && query.hop < hops.length) {
    newURL = hops[query.hop].server +
             "/tests/dom/security/test/cors/file_CrossSiteXHR_server.sjs?" +
             "hop=" + (query.hop + 1) + "&hops=" + escape(query.hops);
    response.setStatusLine(null, 307, "redirect");
    response.setHeader("Location", newURL);

    return;
  }

  // Send response body
  if (!isPreflight && request.method != "HEAD") {
    response.setHeader("Content-Type", "application/xml", false);
    response.write("<res>hello pass</res>\n");
  }
  if (isPreflight && "preflightBody" in query) {
    response.setHeader("Content-Type", "text/plain", false);
    response.write(query.preflightBody);
  }
}

function sendHttp500(response, text) {
  response.setStatusLine(null, 500, text);
}
