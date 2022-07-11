/*
 * Test for asynchronous image load/decode notifications in the case that the image load works.
 */

// A simple 3x3 png; rows go red, green, blue. Stolen from the PNG encoder test.

var pngspec =
  "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAMAAAADCAIAAADZSiLoAAAAEUlEQVQImWP4z8AAQTAamQkAhpcI+DeMzFcAAAAASUVORK5CYII=";
var ioService = Services.io;
var uri = ioService.newURI(pngspec);

load("async_load_tests.js");
