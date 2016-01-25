/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

window.addEventListener("load", function () {
  var ws1 = new WebSocket("ws://0.0.0.0:81");
  ws1.onopen = function() {
    ws1.send("test 1");
    ws1.close();
  };

  var ws2 = new window.frames[0].WebSocket("ws://0.0.0.0:82");
  ws2.onopen = function() {
    ws2.send("test 2");
    ws2.close();
  };
}, false);
