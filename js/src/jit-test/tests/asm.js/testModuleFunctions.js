// |jit-test| test-also-noasmjs
function testUniqueness(asmJSModule) {
    var f = asmJSModule();
    var g = asmJSModule();
    assertEq(f === g, false);
    f.x = 4;
    assertEq(f.x, 4);
    assertEq(g.x, undefined);
}

function deffun() {
    if (Math.sin) {
        function inner() { "use asm"; function g() {} return g }
    }
    return inner;
}

testUniqueness(deffun);

function lambda() {
    var x = function inner() { "use asm"; function g() {} return g };
    return x;
}

testUniqueness(lambda);

function inEval() {
    eval("function inner() { 'use asm'; function g() {} return g }");
    return inner;
}

testUniqueness(inEval);

function inWith() {
    with ({}) {
        function inner() { "use asm"; function g() {} return g }
    }
    return inner;
}

testUniqueness(inWith);
