function handleRequest(request, response) {
  response.setHeader("Cache-Control", "no-cache", false);
  let queryStr = request.queryString;

  response.setStatusLine("1.1", 302, "Found");
  response.setHeader(
    "Location",
    `download_with_content_disposition_header.sjs?${queryStr}`,
    false
  );
}
