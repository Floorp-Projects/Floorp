const CC = Components.Constructor;

// Context structure - we need to set this up properly to pass to setObjectState
const ctx = {
  QueryInterface(iid) {
    if (iid.equals(Ci.nsISupports)) {
      return this;
    }
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },
};

function setRequest(request) {
  setObjectState(key, request);
}
function getRequest() {
  let request;
  getObjectState(v => {
    request = v;
  });
  return request;
}

function handleRequest(request, response) {
  response.processAsync();
  if (request.queryString == "save") {
    // Get the context structure and finish the old request
    getObjectState("context", function (obj) {
      savedCtx = obj.wrappedJSObject;
      request = savedCtx.request;

      response.setHeader("Content-Type", "application/octet-stream", false);
      response.setHeader("Access-Control-Allow-Origin", "*", false);
      response.setHeader("Cache-Control", "no-cache", false);
      response.setStatusLine(request.httpVersion, 200, "OK");

      const input = request.bodyInputStream;
      const output = response.bodyOutputStream;
      let bodyAvail;
      while ((bodyAvail = input.available()) > 0) {
        output.writeFrom(input, bodyAvail);
      }
      response.finish();
    });
    return;
  }

  if (
    request.queryString == "malformedresult=1" ||
    request.queryString == "emptyresult=1"
  ) {
    jsonOK =
      request.queryString == "malformedresult=1"
        ? '{"status":"ok","dat'
        : '{"status":"ok","data":[]}';
    response.setHeader("Content-Length", String(jsonOK.length), false);
    response.setHeader("Content-Type", "application/json", false);
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.setHeader("Cache-Control", "no-cache", false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(jsonOK, jsonOK.length);
    response.finish();
  } else if (request.queryString == "hangup=1") {
    response.finish();
  } else if (request.queryString == "return400=1") {
    jsonOK = "{'message':'Bad header:accept-language-stt'}";
    response.setHeader("Content-Length", String(jsonOK.length), false);
    response.setHeader("Content-Type", "application/json", false);
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.setHeader("Cache-Control", "no-cache", false);
    response.setStatusLine(request.httpVersion, 400, "Bad Request");
    response.write(jsonOK, jsonOK.length);
    response.finish();
  } else {
    ctx.wrappedJSObject = ctx;
    ctx.request = request;
    setObjectState("context", ctx);
    jsonOK = '{"status":"ok","data":[{"confidence":0.9085610,"text":"hello"}]}';
    response.setHeader("Content-Length", String(jsonOK.length), false);
    response.setHeader("Content-Type", "application/json", false);
    response.setHeader("Access-Control-Allow-Origin", "*", false);
    response.setHeader("Cache-Control", "no-cache", false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(jsonOK, jsonOK.length);
    response.finish();
  }
}
