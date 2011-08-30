function raisesException(exception) {
  return function (code) {
      eval(code);
  };
};
function obj() {
  var o = { assertEq: true, y: 1 };
  Object.defineProperty(o, 'x', { writable: false });
  return o;
}
function in_strict_with(expr) {
  return "with(obj()) { (function () { 'use strict'; " + expr + " })(); }";
}
try { assertEq(raisesException(TypeError)(in_strict_with('x++;')), true); } catch (e) {}
