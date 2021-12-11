/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(req, resp) {
  resp.setHeader("Content-Type", "text/html", false);
  if (req.hasHeader("Origin")) {
    resp.write("error");
    return;
  }
  resp.write("hello world");
}
