/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

window.addEventListener("load", function() {
  const ws1 = new WebSocket("ws://0.0.0.0:81");
  ws1.onopen = function() {
    ws1.send("test 1");
    ws1.close();
  };

  const ws2 = new window.frames[0].WebSocket("ws://0.0.0.0:82");
  ws2.onopen = function() {
    ws2.send("test 2");
    ws2.close();
  };
});
