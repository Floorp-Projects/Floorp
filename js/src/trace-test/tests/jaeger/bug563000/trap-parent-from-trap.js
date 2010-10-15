setDebug(true);
x = "notset";

function child() {
  x = "failure1";
  /* JSOP_STOP in parent. */
  trap(parent, 10, "success()");
}

function parent() {
  x = "failure2";
}
/* First op in parent: because of JSOP_BEGIN, it is op 1. */
trap(parent, 1, "child()");

function success() {
  x = "success";
}

parent();
assertEq(x, "success");
