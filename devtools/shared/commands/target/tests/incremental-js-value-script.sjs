function handleRequest(request, response) {
  let Etag = '"4d881ab-b03-435f0a0f9ef00"';
  let IfNoneMatch = request.hasHeader("If-None-Match")
                    ? request.getHeader("If-None-Match")
                    : "";

  var counter = getState("cache-counter") || 1;
  let page = "<script>var jsValue = '" + counter + "';</script>" + counter;

  setState("cache-counter", "" + (parseInt(counter) + 1));

  response.setHeader("Etag", Etag, false);

  if (IfNoneMatch === Etag) {
    response.setStatusLine(request.httpVersion, "304", "Not Modified");
  } else {
    response.setHeader("Content-Type", "text/html; charset=utf-8", false);
    response.setHeader("Content-Length", page.length + "", false);
    response.write(page);
  }
}
