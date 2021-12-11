/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint-disable */

window.addEventListener("DOMContentLoaded", function () {
  var canvas = document.querySelector("canvas");
  var context = canvas.getContext("2d");
  context.strokeStyle = "foobarCanvasCssParser";
});
