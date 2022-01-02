// Custom *.sjs file specifically for the needs of Bug:
// Bug 1394554 - Block toplevel data: URI navigations after redirect

var DATA_URI =
  "data:text/html,<body>toplevel data: URI navigations after redirect should be blocked</body>";

function handleRequest(request, response) {
  // avoid confusing cache behaviors
  response.setHeader("Cache-Control", "no-cache", false);

  response.setStatusLine("1.1", 302, "Found");
  response.setHeader("Location", DATA_URI, false);
}
