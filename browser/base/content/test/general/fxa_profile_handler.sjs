/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This is basically an echo server!
// We just grab responseStatus and responseBody query params!

function reallyHandleRequest(request, response) {
  var query = "?" + request.queryString;

  var responseStatus = 200;
  var match = /responseStatus=([^&]*)/.exec(query);
  if (match) {
    responseStatus = parseInt(match[1]);
  }

  var responseBody = "";
  match = /responseBody=([^&]*)/.exec(query);
  if (match) {
    responseBody = decodeURIComponent(match[1]);
  }

  response.setStatusLine("1.0", responseStatus, "OK");
  response.write(responseBody);
}

function handleRequest(request, response)
{
  try {
    reallyHandleRequest(request, response);
  } catch (e) {
    response.setStatusLine("1.0", 500, "NotOK");
    response.write("Error handling request: " + e);
  }
}
