/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  let params = request.queryString.split("&");
  let status = params.filter((s) => s.contains("sts="))[0].split("=")[1];

  switch (status) {
    case "100":
      response.setStatusLine(request.httpVersion, 101, "Switching Protocols");
      break;
    case "200":
      response.setStatusLine(request.httpVersion, 202, "Created");
      break;
    case "300":
      response.setStatusLine(request.httpVersion, 303, "See Other");
      break;
    case "400":
      response.setStatusLine(request.httpVersion, 404, "Not Found");
      break;
    case "500":
      response.setStatusLine(request.httpVersion, 501, "Not Implemented");
      break;
  }
  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);
  response.write("Hello status code " + status + "!");
}
