/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* exported handleRequest */

"use strict";

function handleRequest(request, response) {
  const { scheme, host, path, queryString } = request;
  const params = queryString.split("&").reduce((acc, item) => {
    const [key, value] = item.split("=");
    acc[key] = value;
    return acc;
  }, {});

  const basePath = path.substr(0, path.lastIndexOf("/") + 1);
  const redirectURL = `${scheme}://${host}${basePath}/${params.file}`;
  const delayMs = params.delay ? parseInt(params.delay) : 2000;

  // SJS code doesn't have a setTimeout so we just busy-loop...
  const start = Date.now();
  while (Date.now() < start + delayMs) continue;

  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");
  response.setHeader("Access-Control-Allow-Origin", "*", false);

  response.setStatusLine(request.httpVersion, 302, "Found");
  response.setHeader("Location", redirectURL);
}
