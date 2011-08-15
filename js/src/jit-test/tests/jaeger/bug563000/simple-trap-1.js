// |jit-test| debug
setDebug(true);
var x = "failure";
function main() { x = "success"; }

/* The JSOP_STOP in main. */
trap(main, 10, "");
main();

assertEq(x, "success");
