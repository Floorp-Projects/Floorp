/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "Och Aye");

  response.setHeader("Set-Cookie", "foo=bar; SameSite=Lax");

  response.write("Hello world!");
}
