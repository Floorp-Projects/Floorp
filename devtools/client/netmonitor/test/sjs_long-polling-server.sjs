"use strict";

const key = "blocked-response";
function setResponse(response) {
  setObjectState(key, response);
}

function getResponse() {
  let response;
  getObjectState(key, v => {
    response = v;
  });
  return response;
}

function handleRequest(request, response) {
  const { queryString } = request;
  if (!queryString) {
    // The default end point will return a blocked response.
    // The response object will be stored and will be released
    // when "?unblock" is called.
    response.processAsync();
    response.setHeader("Content-Type", "text/plain", false);
    response.write("Begin...\n");
    setResponse(response);
  } else if (queryString == "unblock") {
    // unblock the pending response
    getResponse().finish();
    setResponse(null);

    // and return synchronously.
    response.setHeader("Content-Type", "text/plain");
    response.write("ok");
  }
}
