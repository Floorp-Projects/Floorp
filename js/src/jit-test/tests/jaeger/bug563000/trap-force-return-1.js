// |jit-test| debug
setDebug(true);
function main() {
  return "failure";
}
/* JSOP_RETURN in main. */
trap(main, 5, "'success'");
assertEq(main(), "success");
