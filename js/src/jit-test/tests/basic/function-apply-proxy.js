// fun.apply(null, proxy) should not invoke the proxy's Has trap.

var proxy = new Proxy({}, {
    get: function (target, name, proxy) {
        switch (name) {
          case "length":
            return 2;
          case "0":
            return 15;
          case "1":
	    return undefined;
          default:
            assertEq(false, true);
        }
    },
    has: function (target, name) {
        assertEq(false, true);
    }
});
function foo() {
    assertEq(arguments.length, 2);
    assertEq(arguments[0], 15);
    assertEq(1 in arguments, true);
    assertEq(arguments[1], undefined);
}
foo.apply(null, proxy);
