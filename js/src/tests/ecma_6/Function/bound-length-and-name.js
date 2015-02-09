var proxy = new Proxy(function() {}, {
    getOwnPropertyDescriptor(target, name) {
        assertEq(name, "length");
        return {value: 3, configurable: true};
    },

    get(target, name) {
        if (name == "length")
            return 3;
        if (name == "name")
            return "hello world";
        assertEq(false, true);
    }
})

var bound = Function.prototype.bind.call(proxy);
//assertEq(bound.name, "bound hello world");
assertEq(bound.name, "hello world");
assertEq(bound.length, 3);

var fun = function() {};
Object.defineProperty(fun, "name", {value: 1337});
Object.defineProperty(fun, "length", {value: "15"});
bound = fun.bind();
// assertEq(bound.name, "bound ");
assertEq(bound.name, "");
assertEq(bound.length, 0);

Object.defineProperty(fun, "length", {value: Number.MAX_SAFE_INTEGER});
bound = fun.bind();
assertEq(bound.length, Number.MAX_SAFE_INTEGER);

Object.defineProperty(fun, "length", {value: -100});
bound = fun.bind();
assertEq(bound.length, 0);

fun = function f(a, ...b) { };
assertEq(fun.length, 1);
bound = fun.bind();
assertEq(bound.length, 1);

reportCompare(0, 0, 'ok');
