function foo() {
    let v;
    let i = -2000;
    do {
	v = i * -1;
	v += v;
	i++;
    } while (i < 1);
    return Object.is(v, -0);
}
assertEq(foo(), true);
