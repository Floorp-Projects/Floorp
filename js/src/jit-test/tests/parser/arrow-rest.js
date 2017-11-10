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
`, 0);

// default parameter

testThrow(`
function f(x=...a) =>
`, 13);

// rest parameter

testThrow(`
function f(... ...a) =>
`, 15);

// destructuring parameter

testThrow(`
([... ...a)=>
`, 6);

testThrow(`
({...a)=>
`, 6);

testThrow(`
function f([... ...a)=>
`, 16);

testThrow(`
function f({...a)=>
`, 16);

// arrow

testThrow(`
x => ...a)=>
`, 5);

// template literal

testThrow("`${ ...a)=>}`", 4);

// destructing assignment

testThrow(`
var [... ...a)=>
`, 9);

testThrow(`
var {...a)=>
`, 9);

// initializer

testThrow(`
var [a] = ...a)=>
`, 10);

testThrow(`
var {a:a} = ...a)=>
`, 12);

testThrow(`
var a = ...a)=>
`, 8);

// if statement

testThrow(`
if (...a) =>
`, 4);

// for statement

testThrow(`
for (...a)=>
`, 5);

testThrow(`
for (let a in ...a)=>
`, 14);

testThrow(`
for (let a of ...a)=>
`, 14);

testThrow(`
for (; ...a)=>
`, 7);

testThrow(`
for (;; ...a)=>
`, 8);

// case

testThrow(`
switch (x) { case ...a)=>
`, 18);

// return statement

testThrow(`
function f(x) { return ...a)=>
`, 23);

// yield expression

testThrow(`
function* f(x) { yield ...a)=>
`, 23);

// throw statement

testThrow(`
throw ...a) =>
`, 6);

testThrow(`
try {} catch (x if ...a) =>
`, 19);

// class

testThrow(`
class A extends ...a) =>
`, 16);

// conditional expression

testThrow(`
1 ? ...a) =>
`, 4);

testThrow(`
1 ? 2 : ...a) =>
`, 8);

// unary operators

testThrow(`
void ...a) =>
`, 5);

testThrow(`
typeof ...a) =>
`, 7);

testThrow(`
++ ...a) =>
`, 3);

testThrow(`
delete ...a) =>
`, 7);

// new

testThrow(`
new ...a) =>
`, 4);

// member expression

testThrow(`
x[...a) =>
`, 2);

// array literal

testThrow(`
[... ...a) =>
`, 5);

// object literal

testThrow(`
({[...a) =>
`, 3);

testThrow(`
({x: ...a) =>
`, 5);

// assignment

testThrow(`
x = ...a) =>
`, 4);
