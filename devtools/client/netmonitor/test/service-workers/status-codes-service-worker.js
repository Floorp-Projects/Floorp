/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

addEventListener("fetch", function (event) {
  let response = new Response("Service worker response");
  event.respondWith(response);
});
