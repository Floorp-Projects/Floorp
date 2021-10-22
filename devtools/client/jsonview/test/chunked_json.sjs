/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const key = "json-viewer-chunked-response";
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
    response.processAsync();
    setResponse(response);
    response.setHeader("Content-Type", "application/json");
    // Write something so that the JSON viewer app starts loading.
    response.write(" ");
    return;
  }
  const [command, value] = queryString.split("=");
  switch (command) {
    case "write":
      getResponse().write(value);
      break;
    case "close":
      getResponse().finish();
      setResponse(null);
      break;
  }
  response.setHeader("Content-Type", "text/plain");
  response.write("ok");
}
