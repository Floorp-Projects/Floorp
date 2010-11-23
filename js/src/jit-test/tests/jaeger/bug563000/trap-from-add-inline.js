setDebug(true);
x = "notset";
function main() {
  /* The JSOP_STOP in a. */
  a = { valueOf: function () { trap(main, 38, "success()"); } };
  a + "";
  x = "failure";
}
function success() { x = "success"; }

main();
assertEq(x, "success");
