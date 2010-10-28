setDebug(true);
x = "notset";
function main() {
  /* JSOP_STOP in main. */
  untrap(main, 23);
  x = "success";
}
function failure() { x = "failure"; }

/* JSOP_STOP in main. */
trap(main, 23, "failure()");
main();
assertEq(x, "success");
