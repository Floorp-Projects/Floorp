function CanBeConstructed(f) {
    var caught = false;
    try {
        new f;
    } catch (e) {
        caught = true;
    }
    return !caught;
}

function IsConstructedFunction(f) {
    return f.hasOwnProperty('length')
        && f.hasOwnProperty('name')
        && f.hasOwnProperty('prototype')
        && f.prototype.hasOwnProperty('constructor')
        && f.prototype.constructor === f;
}

function IsntConstructedFunction(f) {
    return !f.hasOwnProperty('length')
        && !f.hasOwnProperty('name')
        && !f.hasOwnProperty('prototype')
}

var m = function() {
    "use asm"
    function g(){}
    return g;
};
assertEq(CanBeConstructed(m), true, "asm.js modules can't be constructed");

var objM = new m;
assertEq(IsConstructedFunction(objM), true);

var g = m();
assertEq(CanBeConstructed(g), true, "asm.js functions can't be constructed");
// g is a ctor returning an primitive value, thus an empty object
assertEq(Object.getOwnPropertyNames(new g).length, 0);

var n = function() {
    "use asm"
    function g(){return 42.0}
    function h(){return 42}
    return {
        g: g,
        h: h
    };
};
assertEq(CanBeConstructed(n), true, "asm.js modules can't be constructed");

var objN = new n;
// objN is an object with attributes g and h
assertEq(IsntConstructedFunction(objN), true);
assertEq(objN.hasOwnProperty('g'), true);
assertEq(objN.hasOwnProperty('h'), true);

assertEq(IsConstructedFunction(objN.g), true);
assertEq(IsConstructedFunction(objN.h), true);

var h = n().h;
assertEq(CanBeConstructed(h), true, "asm.js functions can't be constructed");
// h is a ctor returning an primitive value, thus an empty object
assertEq(Object.getOwnPropertyNames(new h).length, 0);

assertEq(typeof(function() {"use asm"; return {}}.prototype) !== 'undefined', true);
