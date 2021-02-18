/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  Components.utils.importGlobalProperties(["URLSearchParams"]);
  let query = new URLSearchParams(request.queryString);
  response.setHeader("Content-Type", "text/html", false);
  response.write(query.get("q"));
}
