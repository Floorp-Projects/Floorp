/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(req, resp) {
  resp.setHeader("Content-Type", "application/octet-stream", false);
  resp.write("hi");
}
