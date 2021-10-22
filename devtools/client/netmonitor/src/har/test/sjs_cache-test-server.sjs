/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 304, "Not Modified");
  response.setHeader(
    "Cache-Control",
    "no-transform,public,max-age=300,s-maxage=900"
  );
  response.setHeader("Expires", "Thu, 01 Dec 2100 20:00:00 GMT");
  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);
  response.write("Hello from cache!");
}
