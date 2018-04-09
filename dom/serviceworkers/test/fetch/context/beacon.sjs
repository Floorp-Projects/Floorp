/*
 * This is based on dom/tests/mochitest/beacon/beacon-originheader-handler.sjs.
 */

function handleRequest(request, response)
{
  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/plain", false);

  // case XHR-REQUEST: the xhr-request tries to query the
  // stored context from the beacon request.
  if (request.queryString == "queryContext") {
    var context = getState("interceptContext");
    // if the beacon already stored the context - return.
    if (context) {
      response.write(context);
      setState("interceptContext", "");
      return;
    }
    // otherwise wait for the beacon request
    response.processAsync();
    setObjectState("sw-xhr-response", response);
    return;
  }

  // case BEACON-REQUEST: get the beacon context and
  // store the context on the server.
  var context = request.queryString;
  setState("interceptContext", context);

  // if there is an xhr-request waiting, return the context now.
  try{
    getObjectState("sw-xhr-response", function(xhrResponse) {
      if (!xhrResponse) {
        return;
      }
      setState("interceptContext", "");
      xhrResponse.write(context);
      xhrResponse.finish();
    });
  } catch(e) {
  }
}
