function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/html");
  response.setHeader("Cache-Control", "no-cache");
  response.write("<body onload='");

  if (!request.hasHeader('Referer')) {
    response.write("window.parent.onloadCount++;");
  }

  if (request.queryString == "newwindow") {
      response.write("if (window.opener) { window.opener.parent.onloadCount++; window.opener.parent.doNextStep(); }");
      response.write("if (!window.opener) window.close();");
      response.write("'>");
  } else {
    response.write("window.parent.doNextStep();'>");
  }

  response.write(request.method + " " + Date.now());
  response.write("</body>");
}
