// Test 1: When constructing x, we shouldn't take the prototype for this.
// it will crash if that happens
evalcx("\
    var x = newGlobal().Object;\
    function f() { return new x; }\
    f();\
    f();\
", newGlobal());
