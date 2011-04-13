function handleRequest(request, response) {
  response.setHeader("Content-Type", "text/html");
  response.write("<body onload='window.parent.onloadCount++'>" +
                 request.method + " " +
		 Date.now() +
		 "</body>");
}
