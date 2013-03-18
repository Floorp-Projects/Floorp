// Arrow functions in direct eval code.

function f(s) {
    var a = 2;
    return eval(s);
}

var c = f("k => a + k");  // closure should see 'a'
assertEq(c(3), 5);
