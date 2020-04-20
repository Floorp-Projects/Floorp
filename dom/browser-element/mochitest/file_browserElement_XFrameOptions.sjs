function handleRequest(request, response)
{
  response.setHeader("X-Frame-Options", request.queryString, false);
  response.setHeader("Content-Type", "text/html", false);

  // Tests rely on this page not being entirely blank, because they take
  // screenshots to determine whether this page was loaded.
  response.write("<html><body>XFrameOptions test<script>alert('finish')</script></body></html>");
}
