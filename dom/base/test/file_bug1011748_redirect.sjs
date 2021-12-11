// SJS handler to redirect the XMLHttpRequest object to file_bug1011748_OK.sjs
function handleRequest(request, response) {
  response.setStatusLine(null, 302, "Moved Temporarily");
  response.setHeader("Location", "file_bug1011748_OK.sjs", false);
}
