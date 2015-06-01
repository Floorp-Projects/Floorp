// Make sure we wrap the new target on CCW construct calls.
var g = newGlobal();

let f = g.eval('(function (expected) { this.accept = new.target === expected; })');

for (let i = 0; i < 1100; i++)
    assertEq(new f(f).accept, true);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
