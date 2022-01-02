"use strict";
/* exported global arithmetic composition chaining nested */

const obj = { b };

function a() {
  return obj;
}

function b() {
  return 2;
}

function arithmetic() {
  debugger;
  a() + b();
}

function composition() {
  debugger;
  b(a());
}

function chaining() {
  debugger;
  a().b();
}

function c() {
  return b();
}

function nested() {
  debugger;
  c();
}
