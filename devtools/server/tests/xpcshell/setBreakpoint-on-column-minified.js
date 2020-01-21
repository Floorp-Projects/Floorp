"use strict";

function other(){ var a = 1; } function test(){ var a = 1; var b = 2; var c = 3; }

function f() {
  other();
  test();
}