"use strict";

dump(" Evaluated debugger script\n");

setImmediate(function () {
  postMessage("debugger script ran");
});
