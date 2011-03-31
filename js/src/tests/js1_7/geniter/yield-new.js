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

reportCompare(true,true);
