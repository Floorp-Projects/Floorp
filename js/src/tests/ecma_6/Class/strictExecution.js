// Classes are always strict mode. Check computed property names as well.

var test = `
class a { constructor() { Object.preventExtensions({}).prop = 0; } }
assertThrowsInstanceOf(() => new a(), TypeError);

function shouldThrow() {
    class b {
        [Object.preventExtensions({}).prop = 4]() { }
        constructor() { }
    }
}
assertThrowsInstanceOf(shouldThrow, TypeError);
`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
