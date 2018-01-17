/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "Och Aye");

  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");

  response.setHeaderNoCheck("Set-Cookie", "bob=true; Max-Age=10; HttpOnly");
  response.setHeaderNoCheck("Set-Cookie", "tom=cool; Max-Age=10; HttpOnly");

  response.setHeader("Content-Type", "text/plain; charset=utf-8", false);

  response.setHeaderNoCheck("Foo-Bar", "baz");
  response.setHeaderNoCheck("Foo-Bar", "baz");
  response.setHeaderNoCheck("Foo-Bar", "");

  response.write("Hello world!");
}
