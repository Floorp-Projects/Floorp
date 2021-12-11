var o = {get a() {
    return eval("5");
}}
assertEq(o.a, 5);
