/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/xml; charset=utf-8", false);
  response.setHeader("Set-Cookie", "name1=test;", true);
  response.write("<label value='test'>Hello From XML!</label>");
}
