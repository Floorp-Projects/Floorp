function f(x) {
    var count = 0;
    for (var i = 0; i < x.length; ++i)
	count++;
    return count;
}
assertEq(f(Error()), 0);
assertEq(f([[]]), 1);
