/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  const Etag = '"4d881ab-b03-435f0a0f9ef00"';
  const IfNoneMatch = request.hasHeader("If-None-Match")
    ? request.getHeader("If-None-Match")
    : "";

  const guid = "xxxxxxxx-xxxx-xxxx-yxxx-xxxxxxxxxxxx".replace(
    /[xy]/g,
    function (c) {
      const r = (Math.random() * 16) | 0;
      const v = c === "x" ? r : (r & 0x3) | 0x8;

      return v.toString(16);
    }
  );

  const page = "<!DOCTYPE html><html><body><h1>" + guid + "</h1></body></html>";

  response.setHeader("Etag", Etag, false);

  if (IfNoneMatch === Etag) {
    response.setStatusLine(request.httpVersion, "304", "Not Modified");
  } else {
    response.setHeader("Content-Type", "text/html; charset=utf-8", false);
    response.setHeader("Content-Length", page.length + "", false);
    response.write(page);
  }
}
