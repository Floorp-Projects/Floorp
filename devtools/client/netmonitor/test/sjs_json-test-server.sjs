/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {

  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");

  response.setHeader("Content-Type", "application/json; charset=utf-8", false);

  // This server checks the name parameter from the request to decide which JSON object to
  // return.
  let params = request.queryString.split("&");
  let name = (params.filter((s) => s.includes("name="))[0] || "").split("=")[1];
  switch (name) {
    case "null":
      response.write("{ \"greeting\": null }");
      break;
    case "nogrip":
      response.write("{\"obj\": {\"type\": \"string\" }}");
      break;
  }
}
