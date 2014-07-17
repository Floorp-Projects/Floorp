setJitCompilerOption("baseline.usecount.trigger", 10);
setJitCompilerOption("ion.usecount.trigger", 20);

var uceFault = function (i) {
    if (i > 98)
        uceFault = function (i) { return true; };
    return false;
};

// Without "use script" in the inner function, the arguments might be
// obersvable.
function inline_notSoEmpty1(a, b, c, d) {
    // This function is not strict, so we might be able to observe its
    // arguments, if somebody ever called fun.arguments inside it.
    return { v: (a.v + b.v + c.v + d.v - 10) / 4 };
}
var uceFault_notSoEmpty1 = eval(uneval(uceFault).replace('uceFault', 'uceFault_notSoEmpty1'));
function notSoEmpty1() {
    var a = { v: i };
    var b = { v: 1 + a.v };
    var c = { v: 2 + b.v };
    var d = { v: 3 + c.v };
    var unused = { v: 4 + d.v };
    var res = inline_notSoEmpty1(a, b, c, d);
    if (uceFault_notSoEmpty1(i) || uceFault_notSoEmpty1(i))
        assertEq(i, res.v);
}

// Check that we can recover objects with their content.
function inline_notSoEmpty2(a, b, c, d) {
    "use strict";
    return { v: (a.v + b.v + c.v + d.v - 10) / 4 };
}
var uceFault_notSoEmpty2 = eval(uneval(uceFault).replace('uceFault', 'uceFault_notSoEmpty2'));
function notSoEmpty2(i) {
    var a = { v: i };
    var b = { v: 1 + a.v };
    var c = { v: 2 + b.v };
    var d = { v: 3 + c.v };
    var unused = { v: 4 + d.v };
    var res = inline_notSoEmpty2(a, b, c, d);
    if (uceFault_notSoEmpty2(i) || uceFault_notSoEmpty2(i))
        assertEq(i, res.v);
}

// Check that we can recover objects with their content.
var argFault_observeArg = function (i) {
    if (i > 98)
        return inline_observeArg.arguments[0];
    return { test : i };
};
function inline_observeArg(obj, i) {
    return argFault_observeArg(i);
}
function observeArg(i) {
    var obj = { test: i };
    var res = inline_observeArg(obj, i);
    assertEq(res.test, i);
}

// Check case where one successor can have multiple times the same predecessor.
function complexPhi(i) {
    var obj = { test: i };
    switch (i) { // TableSwitch
        case 0: obj.test = 0; break;
        case 1: obj.test = 1; break;
        case 2: obj.test = 2; break;
        case 3: case 4: case 5: case 6:
        default: obj.test = i; break;
        case 7: obj.test = 7; break;
        case 8: obj.test = 8; break;
        case 9: obj.test = 9; break;
    }
    assertEq(obj.test, i);
}

// Check case where one successor can have multiple times the same predecessor.
function withinIf(i) {
    var x = undefined;
    if (i > 5) {
        let obj = { foo: i };
        x = obj.foo;
        obj = undefined;
    } else {
        let obj = { bar: i };
        x = obj.bar;
        obj = undefined;
    }
    assertEq(x, i);
}

// Check case where one successor can have multiple times the same predecessor.
function unknownLoad(i) {
    var obj = { foo: i };
    assertEq(obj.bar, undefined);
}


for (var i = 0; i < 100; i++) {
    notSoEmpty1(i);
    notSoEmpty2(i);
    observeArg(i);
    complexPhi(i);
    withinIf(i);
    unknownLoad(i);
}
