"use strict";

function handleRequest(request, response) {
  if (getState("iframe_resource") == "") {
    response.setHeader("Content-Type", "text/html", false);
    response.write("first load");
    setState("iframe_resource", "ok");
  } else {
    response.setHeader("Content-Type", "text/html", false);
    response.write("second load");
    setState("iframe_resource", "");
  }
}
