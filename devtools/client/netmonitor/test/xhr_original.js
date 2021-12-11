"use strict";

function reallydoxhr() {
  const z = new XMLHttpRequest();
  z.open("get", "test-image.png", true);
  z.send();
}

function doxhr() {
  reallydoxhr();
}

window.doxhr = doxhr;
