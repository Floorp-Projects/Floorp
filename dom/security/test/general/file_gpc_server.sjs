"use strict";

function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/html", false);
  response.setHeader("Cache-Control", "no-cache", false);

  var gpc = request.hasHeader("Sec-GPC") ? request.getHeader("Sec-GPC") : "";

  if (gpc === "1") {
    response.write("true");
  } else {
    response.write("false");
  }
}
