x = "notset";
function main() {
  /* The JSOP_STOP in a. */
  trap(main, 27, "success()");
  x = "failure";
}
function success() { x = "success"; }

main();
assertEq(x, "success");
