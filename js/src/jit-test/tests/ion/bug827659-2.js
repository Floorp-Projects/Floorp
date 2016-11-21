// |jit-test| need-for-each

function f(x) {
    return x["__proto__"]
}
var res = 0;
for each(a in [{}, null]) {
    try {
        f(a);
	res += 20;
    } catch (e) {
	assertEq(e instanceof TypeError, true);
	assertEq(e.message.indexOf("is null") > 0, true);
	res += 50;
    }
}
assertEq(res, 70);
