function debug(str) {
//  dump("@@@ push-server " + str + "\n");
}

function handleRequest(request, response)
{
  debug("handling request!");

  const Cc = Components.classes;
  const Ci = Components.interfaces;

  let params = request.getHeader("X-Push-Server");
  debug("params = " + params);

  let xhr =  Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("PUT", params);
  xhr.send();
  xhr.onload = function(e) {
    debug("xhr : " + this.status);
  }
  xhr.onerror = function(e) {
    debug("xhr error: " + e);
  }

  response.setStatusLine(request.httpVersion, "200", "OK");
}
