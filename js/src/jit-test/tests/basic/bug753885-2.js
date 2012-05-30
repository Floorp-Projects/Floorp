// XML-specific classes do not resolve unless JSOPTION_ALLOW_XML is enabled.

assertEq(options().split(",").indexOf("allow_xml") >= 0, true);
options("allow_xml");

var xmlnames = ["XML", "XMLList", "isXMLName", "QName", "Namespace"];
for (var name of xmlnames)
    assertEq(name in this, false);

var globals = Object.getOwnPropertyNames(this);
for (var name of xmlnames)
    assertEq(globals.indexOf(name), -1);

var g = newGlobal('new-compartment');
for (var name of xmlnames)
    assertEq(name in g, false);

// Turn it back on and check that the XML classes magically appear.
options("allow_xml");
assertEq("QName" in this, true);

globals = Object.getOwnPropertyNames(this);
for (var name of xmlnames)
    assertEq(globals.indexOf(name) >= 0, true);
