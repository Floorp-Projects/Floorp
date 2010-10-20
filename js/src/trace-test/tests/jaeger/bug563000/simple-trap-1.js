setDebug(true);
var x = "failure";
function main() { x = "success"; }

/* The JSOP_STOP in a. */
trap(main, 8, "");
main();

assertEq(x, "success");
