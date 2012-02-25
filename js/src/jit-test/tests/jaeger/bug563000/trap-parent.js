// |jit-test| debug
setDebug(true);
x = "notset";
function child() {
  /* JSOP_STOP in parent. */
  trap(parent, 26, "success()");
}
function parent() {
  child();
  x = "failure";
}
function success() {
  x = "success";
}

parent()
assertEq(x, "success");
