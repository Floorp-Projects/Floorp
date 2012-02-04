// |jit-test| debug
setDebug(true);
x = "notset";
function child() {
  /* JSOP_STOP in parent */
  untrap(parent, 16);
  x = "success";
}
function parent() {
  x = "failure";
}
/* JSOP_STOP in parent */
trap(parent, 16, "child()");

parent();
assertEq(x, "success");
