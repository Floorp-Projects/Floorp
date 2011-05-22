// |jit-test| debug
setDebug(true);
var x = "failure";
function main() { x = "success"; }

/* The JSOP_STOP in a. */
trap(main, 10, "");
main();

assertEq(x, "success");
