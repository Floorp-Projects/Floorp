/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  let params = request.queryString.split("&");
  let limit = (params.filter((s) => s.includes("limit="))[0] || "").split("=")[1];

  response.setStatusLine(request.httpVersion, 200, "Och Aye");

  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");
  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);

  response.write("x".repeat(2 * parseInt(limit, 10)));

  response.write("Hello world!");
}
