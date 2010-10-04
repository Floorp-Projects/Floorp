setDebug(true);
function main() {
  return "failure";
}
/* JSOP_RETURN in main. */
trap(main, 4, "'success'");
assertEq(main(), "success");
