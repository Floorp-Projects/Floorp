x = "notset";
function child() {
  /* JSOP_STOP in parent */
  untrap(parent, 11);
  x = "success";
}
function parent() {
  x = "failure";
}
/* JSOP_STOP in parent */
trap(parent, 11, "child()");

parent();
assertEq(x, "success");
