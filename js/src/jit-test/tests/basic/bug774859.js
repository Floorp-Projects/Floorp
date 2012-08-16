gczeal(4,1);
function g()
{
    try {
	return [];
    } catch (e) {}
}
function f()
{
    for (var i=0; i<2; i++) {
	var o = {a: g(),
		 a: g()};
	print(i);
    }
}
f();
