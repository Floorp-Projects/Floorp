function f() {
    let z = (a = (() => 10),
             b = (() => 20)) => {
        return [a, b];
    }

    function g() {
        return 30;
    }

    return [...z(), g];
}

let [a, b, c] = f();

assertEq(a(), 10);
assertEq(b(), 20);
assertEq(c(), 30);
