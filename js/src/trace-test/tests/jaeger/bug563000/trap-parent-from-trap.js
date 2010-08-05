x = "notset";

function child() {
  x = "failure1";
  /* JSOP_STOP in parent. */
  trap(parent, 11, "success()");
}

function parent() {
  x = "failure2";
}
/* First op in parent. */
trap(parent, 1, "child()");

function success() {
  x = "success";
}

parent();
assertEq(x, "success");
