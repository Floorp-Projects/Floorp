/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(req, resp) {
  let suffixes = ["foo", "bar"];
  let data = [req.queryString, suffixes.map(s => req.queryString + s)];
  resp.setHeader("Content-Type", "application/json", false);
  resp.write(JSON.stringify(data));
}
