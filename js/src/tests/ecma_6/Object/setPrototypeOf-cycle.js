// The cycle check in 9.1.2 [[SetPrototypeOf]] does not walk proxies.
// Therefore it's very easy to create prototype chain cycles involving proxies,
// and the spec requires us to accept this.

var t = {};
var p = new Proxy(t, {});
Object.setPrototypeOf(t, p);  // 🙈

// Actually doing anything that searches this object's prototype chain
// technically should not terminate. We instead throw "too much recursion".
for (var obj of [t, p]) {
    assertThrowsInstanceOf(() => "x" in obj, InternalError);
    assertThrowsInstanceOf(() => obj.x, InternalError);
    assertThrowsInstanceOf(() => { obj.x = 1; }, InternalError);
}
reportCompare(0, 0);
