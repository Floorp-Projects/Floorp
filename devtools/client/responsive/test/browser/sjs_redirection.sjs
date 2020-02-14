/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  if (request.queryString === "") {
    response.setStatusLine(request.httpVersion, 302, "Found");
    response.setHeader("Location", "https://"+ request.host + request.path +"?redirected");
  } else {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write(request.getHeader("user-agent"));
  }
}
