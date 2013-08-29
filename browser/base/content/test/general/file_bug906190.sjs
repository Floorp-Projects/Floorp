function handleRequest(request, response) {
  var page = "<!DOCTYPE html><html><body>bug 906190</body></html>";
  var path = "https://test1.example.com/browser/browser/base/content/test/general/";
  var url = path + "file_bug906190_redirected.html";

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(request.httpVersion, "302", "Found");
  response.setHeader("Location", url, false);
  response.write(page);
}
