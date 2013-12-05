let invoked = false;
Object.defineProperty(Array.prototype, '0', {set: function () {
    invoked = true;
}});

let result = [1, 2, 3].slice(1);
assertEq(invoked, false);

let proxy = new Proxy({}, {
    get: function (target, name, proxy) {
        switch (name) {
          case "length":
            return 2;
          case "0":
            return 15;
          case "1":
            // Should not invoke [[Get]] for this hole.
          default:
            assertEq(false, true);
        }
    },
    has: function (target, name) {
        switch (name) {
          case "0":
            return true;
          case "1":
            return false;
          default:
            assertEq(false, true);
        }
    }
})
result = Array.prototype.slice.call(proxy, 0);
assertEq(result.length, 2);
assertEq(0 in result, true);
assertEq(1 in result, false);
assertEq(result[0], 15);
