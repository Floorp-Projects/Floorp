/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function handleRequest(request, response) {
  response.setHeader("Set-Cookie", "tom=cool; Max-Age=10; HttpOnly", true);
  response.setHeader("Set-Cookie", "bob=true; Max-Age=10; HttpOnly", true);
  response.setHeader("Set-Cookie", "foo=bar; Max-Age=10; HttpOnly", true);
  response.write("Hello world!");
}
