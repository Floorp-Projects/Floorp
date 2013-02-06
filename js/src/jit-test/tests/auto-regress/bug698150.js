// Binary: cache/js-dbg-64-b01eb1ba58ce-linux
// Flags: -m -n
//

gczeal(2);
var g1 = newGlobal('same-compartment');
var proxyStr = "Proxy.create(                                    "+
"  { getOwnPropertyDescriptor: function() assertEq(true,false),  "+
"    fix: function() assertEq(true,false), },                    "+
"  Object.prototype                                              "+
");                                                              ";
var proxy1 = g1.eval(proxyStr);
