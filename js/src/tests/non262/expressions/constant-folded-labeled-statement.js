var BUGNUMBER = 1499448;
var summary = "Constant folder should fold labeled statements";

print(BUGNUMBER + ": " + summary);

var code = disassemble(() => { x: 2+2; });

if (typeof reportCompare === "function")
  reportCompare(true, /int8 4/.test(code));
