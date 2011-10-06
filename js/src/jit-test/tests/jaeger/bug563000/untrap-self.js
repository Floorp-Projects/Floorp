// |jit-test| debug
setDebug(true);
x = "notset";
function main() {
  /* JSOP_STOP in main. */
  untrap(main, 24);
  x = "success";
}
dis(main);
function failure() { x = "failure"; }

/* JSOP_STOP in main. */
trap(main, 24, "failure()");
main();
assertEq(x, "success");
