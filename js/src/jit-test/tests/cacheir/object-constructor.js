// Test various inlinable Object constructor calls.

function callNoArgs() {
  for (let i = 0; i < 100; ++i) {
    let obj = Object();

    // Creates a new empty object.
    assertEq(Reflect.getPrototypeOf(obj), Object.prototype);
    assertEq(Reflect.ownKeys(obj).length, 0);
  }
}
for (let i = 0; i < 2; ++i) callNoArgs();

function constructNoArgs() {
  for (let i = 0; i < 100; ++i) {
    let obj = new Object();

    // Creates a new empty object.
    assertEq(Reflect.getPrototypeOf(obj), Object.prototype);
    assertEq(Reflect.ownKeys(obj).length, 0);
  }
}
for (let i = 0; i < 2; ++i) constructNoArgs();

function funCallNoArgs() {
  // NB: Function.prototype.call is only inlined when the thisValue argument is present.
  const thisValue = null;

  for (let i = 0; i < 100; ++i) {
    let obj = Object.call(thisValue);

    // Creates a new empty object.
    assertEq(Reflect.getPrototypeOf(obj), Object.prototype);
    assertEq(Reflect.ownKeys(obj).length, 0);
  }
}
for (let i = 0; i < 2; ++i) funCallNoArgs();

function callObjectArg() {
  let xs = [{}, {}];
  for (let i = 0; i < 100; ++i) {
    let x = xs[i & 1];
    let obj = Object(x);

    // Returns the input object.
    assertEq(obj, x);
  }
}
for (let i = 0; i < 2; ++i) callObjectArg();

function constructObjectArg() {
  let xs = [{}, {}];
  for (let i = 0; i < 100; ++i) {
    let x = xs[i & 1];
    let obj = new Object(x);

    // Returns the input object.
    assertEq(obj, x);
  }
}
for (let i = 0; i < 2; ++i) constructObjectArg();

function funCallObjectArg() {
  // NB: Function.prototype.call is only inlined when the thisValue argument is present.
  const thisValue = null;

  let xs = [{}, {}];
  for (let i = 0; i < 100; ++i) {
    let x = xs[i & 1];
    let obj = Object.call(thisValue, x);

    // Returns the input object.
    assertEq(obj, x);
  }
}
for (let i = 0; i < 2; ++i) funCallObjectArg();
