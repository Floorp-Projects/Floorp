"use strict";

function handleRequest(request, response) {
  // redirect_to.sjs?ctxmenu-image.png
  // redirects to :  ctxmenu-image.png
  const redirectUrl = request.queryString;
  response.setStatusLine(request.httpVersion, "302", "Found");
  response.setHeader("Location", redirectUrl, false);
}
