// XML syntax is not available unless JSOPTION_ALLOW_XML is enabled.

load(libdir + "asserts.js");

var exprs = ["<a/>", "x..y", "x.@id", "x.(@id === '13')", "x.*", "x.function::length",
             "function::parseInt", "a::b"];

assertEq(options().split(",").indexOf("allow_xml") >= 0, true);

options("allow_xml");  // turn it off
for (var e of exprs)
    assertThrowsInstanceOf(function () { Function("return " + e + ";"); }, SyntaxError);

options("allow_xml");  // turn it back on
for (var e of exprs)
    assertEq(typeof Function("return " + e + ";"), "function");
