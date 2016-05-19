/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

window.addEventListener("DOMContentLoaded", function () {
  var canvas = document.querySelector("canvas");
  var context = canvas.getContext("2d");
  context.strokeStyle = "foobarCanvasCssParser";
}, false);
