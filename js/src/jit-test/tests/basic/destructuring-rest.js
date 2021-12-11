
load(libdir + 'asserts.js');
load(libdir + 'eqArrayHelper.js');

assertThrowsInstanceOf(() => new Function('[...a, ,] = []'), SyntaxError, 'trailing elision');
assertThrowsInstanceOf(() => new Function('[a, ...b, c] = []'), SyntaxError, 'trailing param');
assertThrowsInstanceOf(() => new Function('[...a=b] = []'), SyntaxError, 'assignment expression');
assertThrowsInstanceOf(() => new Function('[...a()] = []'), SyntaxError, 'call expression');
assertThrowsInstanceOf(() => new Function('[...(a,b)] = []'), SyntaxError, 'comma expression');
assertThrowsInstanceOf(() => new Function('[...a++] = []'), SyntaxError, 'postfix expression');
assertThrowsInstanceOf(() => new Function('[...!a] = []'), SyntaxError, 'unary expression');
assertThrowsInstanceOf(() => new Function('[...a+b] = []'), SyntaxError, 'binary expression');
assertThrowsInstanceOf(() => new Function('var [...a.x] = []'), SyntaxError, 'lvalue expression in declaration');
assertThrowsInstanceOf(() => new Function('var [...(b)] = []'), SyntaxError);
assertThrowsInstanceOf(() => new Function('[...b,] = []'), SyntaxError);

assertThrowsInstanceOf(() => {
  try {
    eval('let [...[...x]] = (() => { throw "foo"; } )();');
  } catch(e) {
    assertEq(e, "foo");
  }
  x;
}, ReferenceError);

var inputArray = [1, 2, 3];
var inputDeep = [1, inputArray];
var inputObject = {a: inputArray};
var inputStr = 'str';
function *inputGenerator() {
  yield 1;
  yield 2;
  yield 3;
}

var o = {prop: null, call: function () { return o; }};

var expected = [2, 3];
var expectedStr = ['t', 'r'];

function testAll(fn) {
  testDeclaration(fn);

  o.prop = null;
  assertEqArray(fn('[, ...(o.prop)]', inputArray, 'o.prop'), expected);
  o.prop = null;
  assertEqArray(fn('[, ...(o.call().prop)]', inputArray, 'o.prop'), expected);

  o.prop = null;
  assertEqArray(fn('[, ...[...(o.prop)]]', inputArray, 'o.prop'), expected);
  o.prop = null;
  assertEqArray(fn('[, ...[...(o.call().prop)]]', inputArray, 'o.prop'), expected);
}
function testDeclaration(fn) {
  testStr(fn);

  assertEqArray(fn('[, ...rest]', inputArray), expected);
  assertEqArray(fn('[, ...rest]', inputGenerator()), expected);
  assertEqArray(fn('[, [, ...rest]]', inputDeep), expected);
  assertEqArray(fn('{a: [, ...rest]}', inputObject), expected);

  assertEqArray(fn('[, ...[...rest]]', inputArray), expected);
  assertEqArray(fn('[, ...[...rest]]', inputGenerator()), expected);
  assertEqArray(fn('[, [, ...[...rest]]]', inputDeep), expected);
  assertEqArray(fn('{a: [, ...[...rest]]}', inputObject), expected);

  assertEqArray(fn('[, ...{0: a, 1: b}]', inputArray, '[a, b]'), expected);
  assertEqArray(fn('[, ...{0: a, 1: b}]', inputGenerator(), '[a, b]'), expected);
  assertEqArray(fn('[, [, ...{0: a, 1: b}]]', inputDeep, '[a, b]'), expected);
  assertEqArray(fn('{a: [, ...{0: a, 1: b}]}', inputObject, '[a, b]'), expected);
}

function testStr(fn) {
  assertEqArray(fn('[, ...rest]', inputStr), expectedStr);

  assertEqArray(fn('[, ...[...rest]]', inputStr), expectedStr);

  assertEqArray(fn('[, ...{0: a, 1: b}]', inputStr, '[a, b]'), expectedStr);
}

function testForIn(pattern, input, binding) {
  binding = binding || 'rest';
  return new Function('input',
    'for (var ' + pattern + ' in {[input]: 1}) {}' +
    'return ' + binding
  )(input);
}
testStr(testForIn);

function testVar(pattern, input, binding) {
  binding = binding || 'rest';
  return new Function('input',
    'var ' + pattern + ' = input;' +
    'return ' + binding
  )(input);
}
testDeclaration(testVar);

function testGlobal(pattern, input, binding) {
  binding = binding || 'rest';
  return new Function('input',
    '(' + pattern + ' = input);' +
    'return ' + binding
  )(input);
}
testAll(testGlobal);

function testClosure(pattern, input, binding) {
  binding = binding || 'rest';
  const decl = binding.replace('[', '').replace(']', '');
  return new Function('input',
    'var ' + decl + '; (function () {' +
    '(' + pattern + ' = input);' +
    '})();' +
    'return ' + binding
  )(input);
}
testDeclaration(testClosure);

function testArgument(pattern, input, binding) {
  binding = binding || 'rest';
  return new Function('input',
    'return (function (' + pattern + ') {' +
    'return ' + binding + '; })(input);'
  )(input);
}
testDeclaration(testArgument);

function testArgumentFunction(pattern, input, binding) {
  binding = binding || 'rest';
  return new Function(pattern,
    'return ' + binding
  )(input);
}
// ES6 requires the `Function` constructor to accept arbitrary
// `BindingElement`s as formal parameters.
testDeclaration(testArgumentFunction);

function testThrow(pattern, input, binding) {
  binding = binding || 'rest';
  return new Function('input',
    'try { throw input }' +
    'catch(' + pattern + ') {' +
    'return ' + binding + '; }'
  )(input);
}
testDeclaration(testThrow);
