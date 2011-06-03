// If the debuggee cannot be put into debug mode, throw.
var g = newGlobal('new-compartment');
g.libdir = libdir;
g.eval("load(libdir + 'asserts.js');");
g.parent = this;
g.eval("assertThrowsInstanceOf(function () { new Debug(parent); }, Error);");
