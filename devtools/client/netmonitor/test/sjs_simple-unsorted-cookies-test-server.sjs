/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "Och Aye");

  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");

  response.setHeader("Set-Cookie", "tom=cool; Max-Age=10; HttpOnly", true);
  response.setHeader("Set-Cookie", "bob=true; Max-Age=10; HttpOnly", true);
  response.setHeader("Set-Cookie", "foo=bar; Max-Age=10; HttpOnly", true);

  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);
  response.setHeader("Foo-Bar", "baz", false);
  response.write("Hello world!");
}
