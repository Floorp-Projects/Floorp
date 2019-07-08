"use strict";
/* exported global doRet doThrow */

function ret() {
  return 2;
}

function throws() {
  throw new Error("yo");
}

function doRet() {
  debugger;
  const r = ret();
  return r;
}

function doThrow() {
  debugger;
  try {
    throws();
  } catch (e) {}
}
