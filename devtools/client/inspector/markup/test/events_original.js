/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function clickme() {
  console.log("clickme");
}

function init() {
  const s = document.querySelector("#clicky");
  s.addEventListener("click", clickme);
}

window.init = init;
