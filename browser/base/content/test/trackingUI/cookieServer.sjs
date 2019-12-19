/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// A 1x1 PNG image.
// Source: https://commons.wikimedia.org/wiki/File:1x1.png (Public Domain)
const IMAGE = atob("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABAQMAAAAl21bKAAAAA1BMVEUAA" +
                   "ACnej3aAAAAAXRSTlMAQObYZgAAAApJREFUCNdjYAAAAAIAAeIhvDMAAAAASUVORK5CYII=");

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200);
  if (request.queryString &&
      request.queryString.includes("type=image-no-cookie")) {
    response.setHeader("Content-Type", "image/png", false);
    response.write(IMAGE);
  } else {
    response.setHeader("Set-Cookie", "foopy=1");
    response.write("cookie served");
  }
}
