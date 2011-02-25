// |jit-test| debug
setDebug(true);
function main() {
  return 1;
}
/* JSOP_RETURN in main. */
trap(main, 1, "0");
assertEq(main(), 0);
