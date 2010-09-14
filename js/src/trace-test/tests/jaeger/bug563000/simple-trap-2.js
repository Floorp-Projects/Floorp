setDebug(true);
var x = "notset";
function main() { x = "failure"; }
function success() { x = "success"; }

/* The JSOP_STOP in a. */
trap(main, 6, "success()");
main();

assertEq(x, "success");
