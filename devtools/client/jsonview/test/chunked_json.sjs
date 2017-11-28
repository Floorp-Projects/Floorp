/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const key = "json-viewer-chunked-response";
function setResponse(response) {
  setObjectState(key, response);
}
function getResponse() {
  let response;
  getObjectState(key, v => { response = v });
  return response;
}

function handleRequest(request, response) {
  let {queryString} = request;
  if (!queryString) {
    response.processAsync();
    setResponse(response);
    response.setHeader("Content-Type", "application/json");
    // Write something so that the JSON viewer app starts loading.
    response.write(" ");
    return;
  }
  let [command, value] = queryString.split('=');
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
