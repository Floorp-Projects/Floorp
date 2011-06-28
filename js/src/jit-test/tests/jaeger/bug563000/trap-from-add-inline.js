// |jit-test| debug
setDebug(true);
x = "notset";
function main() {
  /* The JSOP_STOP in main. */
  a = { valueOf: function () { trap(main, 36, "success()"); } };
  a + "";
  x = "failure";
}
function success() { x = "success"; }

main();
assertEq(x, "success");
