// Classes are always strict mode. Check computed property names and heritage
// expressions as well.

class a { constructor() { Object.preventExtensions({}).prop = 0; } }
assertThrowsInstanceOf(() => new a(), TypeError);
var aExpr = class { constructor() { Object.preventExtensions().prop = 0; } };
assertThrowsInstanceOf(() => new aExpr(), TypeError);

function shouldThrowCPN() {
    class b {
        [Object.preventExtensions({}).prop = 4]() { }
        constructor() { }
    }
}
function shouldThrowCPNExpr() {
    var b = class {
        [Object.preventExtensions({}).prop = 4]() { }
        constructor() { }
    };
}
assertThrowsInstanceOf(shouldThrowCPN, TypeError);
assertThrowsInstanceOf(shouldThrowCPNExpr, TypeError);

function shouldThrowHeritage() {
    class b extends (Object.preventExtensions({}).prop = 4) {
        constructor() { }
    }
}
function shouldThrowHeritageExpr() {
    var b = class extends (Object.preventExtensions({}).prop = 4) {
        constructor() { }
    };
}
assertThrowsInstanceOf(shouldThrowHeritage, TypeError);
assertThrowsInstanceOf(shouldThrowHeritageExpr, TypeError);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
