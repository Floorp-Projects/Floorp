
load(libdir + 'asserts.js');
load(libdir + 'eqArrayHelper.js');

var arrayPattern = '[a = 1, b = 2, c = 3, d = 4, e = 5, f = 6]';
var objectPattern = '{0: a = 1, 1: b = 2, 2: c = 3, 3: d = 4, 4: e = 5, 5: f = 6}';
var objectPatternShorthand = '{a = 1, b = 2, c = 3, d = 4, e = 5, f = 6}';
var nestedPattern = '{a: a = 1, b: [b = 2] = [], c: {c: [c]} = {c: [3]}, d: {d, e} = {d: 4, e: 5}, f: f = 6}';

function testAll(fn) {
  assertEqArray(fn(arrayPattern, []), [1, 2, 3, 4, 5, 6]);
  assertEqArray(fn(arrayPattern, [2, 3, 4, 5, 6, 7, 8, 9]), [2, 3, 4, 5, 6, 7]);
  assertEqArray(fn(arrayPattern, [undefined, 0, false, null, "", undefined]), [1, 0, false, null, "", 6]);
  assertEqArray(fn(arrayPattern, [0, false]), [0, false, 3, 4, 5, 6]);

  assertEqArray(fn(objectPattern, []), [1, 2, 3, 4, 5, 6]);
  assertEqArray(fn(objectPattern, [2, 3, 4, 5, 6, 7, 8, 9]), [2, 3, 4, 5, 6, 7]);
  assertEqArray(fn(objectPattern, [undefined, 0, false, null, "", undefined]), [1, 0, false, null, "", 6]);
  assertEqArray(fn(objectPattern, [0, false]), [0, false, 3, 4, 5, 6]);

  assertEqArray(fn(objectPatternShorthand, {}), [1, 2, 3, 4, 5, 6]);
  assertEqArray(fn(objectPatternShorthand, {a: 2, b: 3, c: 4, d: 5, e: 6, f: 7, g: 8, h: 9}), [2, 3, 4, 5, 6, 7]);
  assertEqArray(fn(objectPatternShorthand, {a: undefined, b: 0, c: false, d: null, e: "", f: undefined}),
                   [1, 0, false, null, "", 6]);
  assertEqArray(fn(objectPatternShorthand, {a: 0, b: false}), [0, false, 3, 4, 5, 6]);
  assertEqArray(fn(nestedPattern, {}), [1, 2, 3, 4, 5, 6]);
  assertEqArray(fn(nestedPattern, {a: 2, b: [], c: undefined}), [2, 2, 3, 4, 5, 6]);
  assertEqArray(fn(nestedPattern, {a: undefined, b: [3], c: {c: [4]}}), [1, 3, 4, 4, 5, 6]);
  assertEqArray(fn(nestedPattern, {a: undefined, b: [3], c: {c: [4]}, d: {d: 5, e: 6}}), [1, 3, 4, 5, 6, 6]);
}

function testVar(pattern, input) {
  return new Function('input',
    'var ' + pattern + ' = input;' +
    'return [a, b, c, d, e, f];'
  )(input);
}
testAll(testVar);

function testLet(pattern, input) {
  return new Function('input',
    'let ' + pattern + ' = input;' +
    'return [a, b, c, d, e, f];'
  )(input);
}
testAll(testLet);

function testConst(pattern, input) {
  return new Function('input',
    'const ' + pattern + ' = input;' +
    'return [a, b, c, d, e, f];'
  )(input);
}
testAll(testConst);

function testGlobal(pattern, input) {
  return new Function('input',
    '(' + pattern + ' = input);' +
    'return [a, b, c, d, e, f];'
  )(input);
}
testAll(testGlobal);

function testClosure(pattern, input) {
  return new Function('input',
    'var rest; (function () {' +
    '(' + pattern + ' = input);' +
    '})();' +
    'return [a, b, c, d, e, f];'
  )(input);
}
testAll(testClosure);

function testArgument(pattern, input) {
  return new Function('input',
    'return (function (' + pattern + ') {' +
    'return [a, b, c, d, e, f]; })(input);'
  )(input);
}
testAll(testArgument);

function testArgumentFunction(pattern, input) {
  return new Function(pattern,
    'return [a, b, c, d, e, f];'
  )(input);
}
testAll(testArgumentFunction);

function testThrow(pattern, input) {
  return new Function('input',
    'try { throw input }' +
    'catch(' + pattern + ') {' +
    'return [a, b, c, d, e, f]; }'
  )(input);
}
testAll(testThrow);

// test global const
const [ca = 1, cb = 2] = [];
assertEq(ca, 1);
assertEq(cb, 2);

const {a: {a: cc = 3} = {a: undefined}} = {};
assertEq(cc, 3);

// test that the assignment happens in source order
var a = undefined, b = undefined, c = undefined;
({a: a = 1, c: c = 2, b: b = 3} = {
  get a() {
    assertEq(a, undefined);
    assertEq(c, undefined);
    assertEq(b, undefined);
    return undefined;
  },
  get b() {
    assertEq(a, 1);
    assertEq(c, 2);
    assertEq(b, undefined);
    return 4;
  },
  get c() {
    assertEq(a, 1);
    assertEq(c, undefined);
    assertEq(b, undefined);
    return undefined;
  }
});
assertEq(b, 4);

assertThrowsInstanceOf(() => { var {a: {a} = null} = {}; }, TypeError);
assertThrowsInstanceOf(() => { var [[a] = 2] = []; }, TypeError);

// destructuring assignment might have  duplicate variable names.
var [a = 1, a = 2] = [3];
assertEq(a, 2);

// assignment to properties of default params
[a = {y: 2}, a.x = 1] = [];
assertEq(typeof a, 'object');
assertEq(a.x, 1);
assertEq(a.y, 2);

// defaults are evaluated even if there is no binding
var evaled = false;
({a: {} = (evaled = true, {})} = {});
assertEq(evaled, true);
evaled = false;
assertThrowsInstanceOf(() => { [[] = (evaled = true, 2)] = [] }, TypeError);
assertEq(evaled, true);

assertThrowsInstanceOf(() => new Function('var [...rest = defaults] = [];'), SyntaxError);

new Function(`
  b = undefined;
  for (var [a, b = 10] in " ") {}
  assertEq(b, 10);`)();

new Function(`
  b = undefined;
  for (let [a, c = 10] in " ") { b = c; }
  assertEq(b, 10);`)();

new Function(`
  b = undefined;
  for (let [a, c = (x => y)] in " ") { b = c; }
  assertEq(typeof b, "function");`)();

new Function(`
  b = undefined;
  for (let [a, __proto__ = 10] in " ") { b = __proto__; }
  assertEq(b, 10);`)();

new Function(`
  b = undefined;
  for (let [a, __proto__ = (x => y)] in " ") { b = __proto__; }
  assertEq(typeof b, "function");`)();

new Function(`
  b = undefined;
  for (var {1: b = 10} in " ") {}
  assertEq(b, 10);`)();

new Function(`
  b = undefined;
  for (let {1: c = 10} in " ") { b = c; }
  assertEq(b, 10);`)();

new Function(`
  b = undefined;
  for (let { c = 10 } in " ") { b = c; }
  assertEq(b, 10);`)();

new Function(`
  b = undefined;
  assertEq(Number.prototype.a, undefined);
  for (var { a: c = (x => y) } in [{}]) { b = c; }
  assertEq(typeof b, "function");`)();

new Function(`
  b = undefined;
  Object.defineProperty(String.prototype, "__proto__",
                        { value: undefined, configurable: true });
  for (var { __proto__: c = (x => y) } in [{}]) { b = c; }
  delete String.prototype.__proto__;
  assertEq(typeof b, "function");`)();

new Function(`
  b = undefined;
  for (var { a: c = (x => y) } of [{ a: undefined }]) { b = c; }
  assertEq(typeof b, "function");`)();

new Function(`
  b = undefined;
  for (var { __proto__: c = (x => y) } of [{ ["__proto__"]: undefined }]) { b = c; }
  assertEq(typeof b, "function");`)();

new Function(`
  b = undefined;
  var ts = Function.prototype.toString;
  Function.prototype.toString = () => 'hi';
  String.prototype.hi = 42;
  for (var { [(x => y)]: c } in [0]) { b = c; }
  Function.prototype.toString = ts;
  delete String.prototype.hi;
  assertEq(b, 42);`)();

new Function(`
  b = undefined;
  var ts = Function.prototype.toString;
  Function.prototype.toString = () => 'hi';
  String.prototype.hi = 42;
  for (var { [(x => y)]: __proto__ } in [0]) { b = __proto__; }
  Function.prototype.toString = ts;
  delete String.prototype.hi;
  assertEq(b, 42);`)();

new Function(`
  b = undefined;
  var ts = Function.prototype.toString;
  Function.prototype.toString = () => 'hi';
  for (var { [(x => y)]: c } of [{ 'hi': 42 }]) { b = c; }
  Function.prototype.toString = ts;
  assertEq(b, 42);`)();

new Function(`
  b = undefined;
  var ts = Function.prototype.toString;
  Function.prototype.toString = () => 'hi';
  for (var { [(x => y)]: __proto__ } of [{ hi: 42 }]) { b = __proto__; }
  Function.prototype.toString = ts;
  assertEq(b, 42);`)();
