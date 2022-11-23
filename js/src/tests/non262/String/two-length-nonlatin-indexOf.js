var BUGNUMBER = 1801690;
var summary = "indexOf function doesn't work correctly with polish letters";

// Prior to this bug being fixed, this would return 0. This is because 'ł'
// truncates to the same 8-bit number as 'B'. We had a guard on the first
// character, but there was a hole in our logic specifically for the
// second character of the needle string.
assertEq("AB".indexOf("Ał"), -1);

if (typeof reportCompare === "function")
  reportCompare(true, true);
