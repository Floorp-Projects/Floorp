// |jit-test| debug
setDebug(true);
x = "notset";
function main() {
  /* The JSOP_STOP in main. */
  trap(main, 28, "success()");
  x = "failure";
}
function success() { x = "success"; }

main();
assertEq(x, "success");
