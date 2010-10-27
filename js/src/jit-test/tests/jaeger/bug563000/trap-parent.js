setDebug(true);
x = "notset";
function child() {
  /* JSOP_STOP in parent. */
  trap(parent, 17, "success()");
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
