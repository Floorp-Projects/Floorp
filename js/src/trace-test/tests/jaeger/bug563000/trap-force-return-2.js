function main() {
  return 1;
}
/* JSOP_RETURN in main. */
trap(main, 2, "0");
assertEq(main(), 0);
