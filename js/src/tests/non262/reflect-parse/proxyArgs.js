// |reftest| skip-if(!xulRuntime.shell)
// |reftest| skip-if(!xulRuntime.shell)
// bug 905774

// Proxy options
var opts = new Proxy({loc: false}, {});
assertEq("loc" in Reflect.parse("0;", opts), false);
opts.loc = true;
assertEq(Reflect.parse("0;", opts).loc !== null, true);
delete opts.loc;
assertEq(Reflect.parse("0;", opts).loc !== null, true);  // default is true

reportCompare(0, 0, 'ok');
