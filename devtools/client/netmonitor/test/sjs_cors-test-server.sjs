/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "Och Aye");

  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");

  response.setHeader("Access-Control-Allow-Origin", "*", false);
  response.setHeader("Access-Control-Allow-Headers", "content-type", false);

  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);

  response.write("Access-Control-Allow-Origin: *");
}
