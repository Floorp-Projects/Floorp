/* Don't assert. */
x = <x/>
function f(aaa) {
    aaa.e = x
}
for each(let c in [x, x, x]) {
    f(c)
}
assertEq(0,0);
