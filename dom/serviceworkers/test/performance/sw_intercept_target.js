"use strict";

onfetch = function (event) {
  if (event.request.url.indexOf("target.txt") != -1) {
    event.respondWith(fetch("intercepted.txt"));
  }
};
