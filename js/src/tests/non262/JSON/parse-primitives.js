var x;

// check an empty object, just for sanity
var emptyObject = "{}";
x = JSON.parse(emptyObject);
assertEq(typeof x, "object");
assertEq(x instanceof Object, true);

x = JSON.parse(emptyObject);
assertEq(typeof x, "object");

// booleans and null
x = JSON.parse("true");
assertEq(x, true);

x = JSON.parse("true          ");
assertEq(x, true);

x = JSON.parse("false");
assertEq(x, false);

x = JSON.parse("           null           ");
assertEq(x, null);

// numbers
x = JSON.parse("1234567890");
assertEq(x, 1234567890);

x = JSON.parse("-9876.543210");
assertEq(x, -9876.543210);

x = JSON.parse("0.123456789e-12");
assertEq(x, 0.123456789e-12);

x = JSON.parse("1.234567890E+34");
assertEq(x, 1.234567890E+34);

x = JSON.parse("      23456789012E66          \r\r\r\r      \n\n\n\n ");
assertEq(x, 23456789012E66);

// strings
x = JSON.parse('"foo"');
assertEq(x, "foo");

x = JSON.parse('"\\r\\n"');
assertEq(x, "\r\n");

x = JSON.parse('  "\\uabcd\uef4A"');
assertEq(x, "\uabcd\uef4A");

x = JSON.parse('"\\uabcd"  ');
assertEq(x, "\uabcd");

x = JSON.parse('"\\f"');
assertEq(x, "\f");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
