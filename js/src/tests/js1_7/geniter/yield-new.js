const K = 20;

var obj;

var g = new function() {
    this.x = "puppies";
    obj = this;
    for (var i = 0; i < K; ++i)
        yield i;
    yield this;
}

var ct = 0;
for (var i in g)
    assertEq((ct < K && ct++ == i) || i == obj, true);
assertEq(i.x, "puppies");

function g2() {
    for (var i=0; i<20; i++)
	yield i;
}
var i = 0;
for (var x of new g2()) {
    assertEq(i, x);
    i++;
}

reportCompare(true,true);
