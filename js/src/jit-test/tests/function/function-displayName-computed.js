// |jit-test| skip-if: isLcovEnabled()
// Guessed Atoms tests.
// Test String literals
var obj = {
  ["func"]: function() {return function() {} },
  ["arrowFunc"]: ()=>{return function() {} },
  ["method"]() {return function() {} },
  ["nonAnonymousFunc"]: function F() {return function() {} },
  get ["getter"]() {return function() {} },
};


assertEq(displayName(obj.func()), "func/<");
assertEq(displayName(obj.arrowFunc()), "arrowFunc/<");
assertEq(displayName(obj.method()), "method/<");
assertEq(displayName(obj.nonAnonymousFunc()), "F/<");

// We don't support guessed name for accessor
assertEq(displayName(obj.getter), "");

let dummy = class {
  ["func"]() {return function() {} }
  ["arrowFunc"] = ()=>{return function() {} };
  ["method"]() {return function() {} }
  get ["getter"]() {return function() {} }
};

dum = new dummy();
assertEq(displayName(dum.func()), "func/<");
// We don't support guessed name for class field with computed name
assertEq(displayName(dum.arrowFunc()), "dummy</</<");
assertEq(displayName(dum.method()), "method/<");
// We don't support guessed name for accessor
assertEq(displayName(dummy.prototype.getter), "dummy</<");


// Test numeric literals
var objN = {
  [1]: function() {return function() {}},
  [5]: ()=>{return function() {}},
  [7] () {return function() {}},
  [8]: class {},
  [9]: function F() {return function() {}},
  [10]: class C{},
  get [11]() {return function() {}},
};
assertEq(displayName(objN[1]()), "1/<");
assertEq(displayName(objN[5]()), "5/<");
assertEq(displayName(objN[7]()), "7/<");
assertEq(displayName(objN[8]), "8");
assertEq(displayName(objN[9]()), "F/<");
assertEq(displayName(objN[10]), "C");
// We don't support guessed name for accessor
assertEq(displayName(objN[11]), "objN/<");

let foo = class {
  [1] () {return function() {} }
  [5] = ()=>{return function() {} };
  [7] () {return function() {} }
  get [11]() {return function() {} }
};

f = new foo();

assertEq(displayName(f[1]()), "1/<");
// We don't support guessed name for class field with computed name
assertEq(displayName(f[5]()), "foo</</<");
assertEq(displayName(f[7]()), "7/<");
assertEq(displayName(f[11]), "foo</<");

// Native built-in accessor name, which is lazily created.

assertEq(displayName(Object.getOwnPropertyDescriptor(Map.prototype, "size").get), "get size");
assertEq(displayName(Object.getOwnPropertyDescriptor(RegExp.prototype, "flags").get), "get flags");
