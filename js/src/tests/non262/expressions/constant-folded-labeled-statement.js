var BUGNUMBER = 1499448;
var summary = "Constant folder should fold labeled statements";

print(BUGNUMBER + ": " + summary);

if (typeof disassemble === "function") {
    var code = disassemble(() => { x: 2+2; });

    if (typeof reportCompare === "function")
        reportCompare(true, /Int8 4/.test(code));
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
