function handleRequest(request, response) {
  var page = "<!DOCTYPE html><html><body>bug 906190</body></html>";
  var path =
    "https://test1.example.com/browser/browser/base/content/test/siteIdentity/";
  var url;

  if (request.queryString.includes("bad-redirection=1")) {
    url = path + "this_page_does_not_exist.html";
  } else {
    url = path + "file_bug906190_redirected.html";
  }

  response.setHeader("Cache-Control", "no-cache", false);
  response.setHeader("Content-Type", "text/html", false);
  response.setStatusLine(request.httpVersion, "302", "Found");
  response.setHeader("Location", url, false);
  response.write(page);
}
