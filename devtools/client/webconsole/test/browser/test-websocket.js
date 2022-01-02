/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

window.addEventListener("load", function() {
  const ws1 = new WebSocket("wss://0.0.0.0:81");
  ws1.onopen = function() {
    ws1.send("test 1");
    ws1.close();
  };

  const ws2 = new window.frames[0].WebSocket("wss://0.0.0.0:82");
  ws2.onopen = function() {
    ws2.send("test 2");
    ws2.close();
  };
});
