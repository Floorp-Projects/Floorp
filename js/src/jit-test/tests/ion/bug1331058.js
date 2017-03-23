function foo(o, trigger) {
    bar = function() { return o.getY(); };
    if (trigger)
	assertEq(bar(), undefined);
    return 1;
}
function O(o, trigger) {
    this.a1 = 1;
    this.a2 = 2;
    this.a3 = 3;
    this.a4 = 4;
    this.x = foo(this, trigger);
}
O.prototype.getY = function() {
    return this.x;
}
function test() {
    with(this) {}; // No Ion.
    var arr = [];
    for (var i=0; i<100; i++)
	arr.push(new O({y: i}, false));

    for (var i=0; i<100; i++)
	bar();

    for (var i=0; i<300; i++)
	arr.push(new O({y: i}, true));
}
test();
