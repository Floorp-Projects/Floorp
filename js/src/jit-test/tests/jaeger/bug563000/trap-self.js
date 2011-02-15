// |jit-test| debug
setDebug(true);
x = "notset";
function main() {
  /* The JSOP_STOP in a. */
  trap(main, 25, "success()");
  x = "failure";
}
function success() { x = "success"; }

main();
assertEq(x, "success");
