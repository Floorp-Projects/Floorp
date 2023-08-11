// The parser should throw SyntaxError immediately if it finds "..." in a
// context where it's not allowed.

function testThrow(code, column) {
  var caught = false;
  try {
    eval(code);
  } catch (e) {
    caught = true;
    assertEq(e.columnNumber, column);
  }
  assertEq(caught, true,
           "failed to throw evaluating <" + code + ">");
}

// expression statement

testThrow(`
...a)=>
`, 1);

// default parameter

testThrow(`
function f(x=...a) =>
`, 14);

// rest parameter

testThrow(`
function f(... ...a) =>
`, 16);

// destructuring parameter

testThrow(`
([... ...a)=>
`, 7);

testThrow(`
({...a)=>
`, 7);

testThrow(`
function f([... ...a)=>
`, 17);

testThrow(`
function f({...a)=>
`, 17);

// arrow

testThrow(`
x => ...a)=>
`, 6);

// template literal

testThrow("`${ ...a)=>}`", 5);

// destructing assignment

testThrow(`
var [... ...a)=>
`, 10);

testThrow(`
var {...a)=>
`, 10);

// initializer

testThrow(`
var [a] = ...a)=>
`, 11);

testThrow(`
var {a:a} = ...a)=>
`, 13);

testThrow(`
var a = ...a)=>
`, 9);

// if statement

testThrow(`
if (...a) =>
`, 5);

// for statement

testThrow(`
for (...a)=>
`, 6);

testThrow(`
for (let a in ...a)=>
`, 15);

testThrow(`
for (let a of ...a)=>
`, 15);

testThrow(`
for (; ...a)=>
`, 8);

testThrow(`
for (;; ...a)=>
`, 9);

// case

testThrow(`
switch (x) { case ...a)=>
`, 19);

// return statement

testThrow(`
function f(x) { return ...a)=>
`, 24);

// yield expression

testThrow(`
function* f(x) { yield ...a)=>
`, 24);

// throw statement

testThrow(`
throw ...a) =>
`, 7);

// class

testThrow(`
class A extends ...a) =>
`, 17);

// conditional expression

testThrow(`
1 ? ...a) =>
`, 5);

testThrow(`
1 ? 2 : ...a) =>
`, 9);

// unary operators

testThrow(`
void ...a) =>
`, 6);

testThrow(`
typeof ...a) =>
`, 8);

testThrow(`
++ ...a) =>
`, 4);

testThrow(`
delete ...a) =>
`, 8);

// new

testThrow(`
new ...a) =>
`, 5);

// member expression

testThrow(`
x[...a) =>
`, 3);

// array literal

testThrow(`
[... ...a) =>
`, 6);

// object literal

testThrow(`
({[...a) =>
`, 4);

testThrow(`
({x: ...a) =>
`, 6);

// assignment

testThrow(`
x = ...a) =>
`, 5);
