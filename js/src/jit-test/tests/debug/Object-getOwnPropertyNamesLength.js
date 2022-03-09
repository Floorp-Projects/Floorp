// Basic getOwnPropertyNamesLength tests.

var g = newGlobal({ newCompartment: true });
var dbg = Debugger();
var gobj = dbg.addDebuggee(g);

function testGetOwnPropertyLength(code) {
  code = `(${code});`;
  const expected = Object.getOwnPropertyNames(eval(code)).length;
  g.eval(`obj =  ${code}`);
  const length = gobj
    .getOwnPropertyDescriptor("obj")
    .value.getOwnPropertyNamesLength();
  assertEq(length, expected, `Expected result for: ${code}`);
}

testGetOwnPropertyLength("{}");
testGetOwnPropertyLength("{a: 0, b: 1}");
testGetOwnPropertyLength("{'name with space': 0}");
testGetOwnPropertyLength("{get x() {}, set y(v) {}}");
testGetOwnPropertyLength("{get x() { throw 'fit'; }}");
testGetOwnPropertyLength("Object.create({a: 1})");
testGetOwnPropertyLength("Object.create({get a() {}, set a(v) {}})");
testGetOwnPropertyLength(
  "(function () { var x = {a: 0, b: 1}; delete a; return x; })()"
);
testGetOwnPropertyLength("Object.create(null, {x: {value: 0}})");
testGetOwnPropertyLength("[]");
testGetOwnPropertyLength("[0, 1, 2]");
testGetOwnPropertyLength("[,,,,,]");
testGetOwnPropertyLength("/a*a/");
testGetOwnPropertyLength("function () {}");
testGetOwnPropertyLength(
  `(function () {
      var x = {};
      x[Symbol()] = 1; 
      x[Symbol.for('moon')] = 2; 
      x[Symbol.iterator] = 3;
      return x;
    })()`
);
