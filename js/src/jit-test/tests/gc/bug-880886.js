// |jit-test| error: too much recursion

function testUniqueness(asmJSModule) {
    var f = asmJSModule();
}
function lambda() {
    var x = function inner() { "use asm"; function g() {} return g };
    return lambda();
}
testUniqueness(lambda);
