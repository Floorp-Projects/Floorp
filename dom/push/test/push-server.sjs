function debug(str) {
//  dump("@@@ push-server " + str + "\n");
}

function concatUint8Arrays(arrays, size) {
  let index = 0;
  return arrays.reduce((result, a) => {
    result.set(new Uint8Array(a), index);
    index += a.byteLength;
    return result;
  }, new Uint8Array(size));
}

function handleRequest(request, response)
{
  debug("handling request!");

  const Cc = Components.classes;
  const Ci = Components.interfaces;

  let params = request.getHeader("X-Push-Server");
  debug("params = " + params);

  let xhr =  Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
  xhr.open(request.getHeader("X-Push-Method"), params);

  for (let headers = request.headers; headers.hasMoreElements();) {
    let header = headers.getNext().QueryInterface(Ci.nsISupportsString).data;
    if (header.toLowerCase() != "x-push-server") {
      xhr.setRequestHeader(header, request.getHeader(header));
    }
  }

  let bodyStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(Ci.nsIBinaryInputStream);
  bodyStream.setInputStream(request.bodyInputStream);
  let size = 0;
  let data = [];
  for (let available; available = bodyStream.available();) {
    data.push(bodyStream.readByteArray(available));
    size += available;
  }

  function reply(statusCode, statusText) {
    response.setStatusLine(request.httpVersion, statusCode, statusText);
    response.finish();
  }

  xhr.onload = function(e) {
    debug("xhr : " + this.status);
    reply(this.status, this.statusText);
  };
  xhr.onerror = function(e) {
    debug("xhr error: " + e);
    reply(500, "Internal Server Error");
  };

  response.processAsync();
  xhr.send(concatUint8Arrays(data, size));
}
