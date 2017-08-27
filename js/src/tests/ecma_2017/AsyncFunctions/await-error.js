var BUGNUMBER = 1317153;
var summary = "await outside of async function should provide better error";

print(BUGNUMBER + ": " + summary);

let caught = false;
try {
    eval("await 10");
} catch(e) {
    assertEq(e.message.includes("await is only valid in"), true);
    caught = true;
}
assertEq(caught, true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
