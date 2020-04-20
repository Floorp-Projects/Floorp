function handleRequest(request, response)
{
  var content = 'step 1';
  if (request.queryString == "iframe1") {
      response.setHeader("X-Frame-Options", "Allow-From http://mochi.test:8888/")
      content = 'finish';
  } else {
      response.setHeader("X-Frame-Options", "Allow-From http://example.com")
  }

  response.setHeader("Content-Type", "text/html", false);

  // Tests rely on this page not being entirely blank, because they take
  // screenshots to determine whether this page was loaded.
  response.write("<html><body>XFrameOptions test<script>alert('" + content + "')</script></body></html>");
}
