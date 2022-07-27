/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";
// small red image
const IMG_BYTES = atob(
  "iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12" +
    "P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg=="
);

function handleRequest(request, response) {
  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: image/png\r\n");
  if (request.queryString === "corp_cross_origin") {
    response.write("Cross-Origin-Resource-Policy: cross-origin\r\n");
  }
  response.write("\r\n");
  response.write(IMG_BYTES);
  response.finish();
}
