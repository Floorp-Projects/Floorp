function get(x) {
    return x;
}

function f(x) {
    switch(x) {
    case get(0):
        return 0;
    case get(1):
        return 1;
    case get("foo"):
        return "foo";
    case get(true):
        return true;
    case get(false):
        return false;
    case get({}):
        return {};
    case get(null):
        return null;
    case Number.MIN_VALUE:
        return Number.MIN_VALUE;
    case Math:
        return Math;
    default:
        return -123;
    }
}

assertEq(f(0), 0);
assertEq(f(-0), 0);
assertEq(f(1), 1);
assertEq(f("foo"), "foo");
assertEq(f(true), true);
assertEq(f(false), false);
assertEq(f({}), -123);
assertEq(f([]), -123);
assertEq(f(Math), Math);

assertEq(f({x:1}), -123);
assertEq(f(333), -123);
assertEq(f(null), null);
assertEq(f(undefined), -123);

assertEq(f(Number.MIN_VALUE), Number.MIN_VALUE);

