"use strict";

function handleRequest(request, response) {
  if (getState("expired_resource") == "") {
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Cache-Control", "max-age=1001");
    setState("expired_resource", "ok");
  } else {
    response.setHeader("Content-Type", "text/html", false);
    response.setHeader("Cache-Control", "max-age=1003");
    setState("expired_resource", "");
  }
}
