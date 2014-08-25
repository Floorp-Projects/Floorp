
load(libdir + 'asserts.js');
load(libdir + 'eqArrayHelper.js');

assertThrowsInstanceOf(() => new Function('[...a, ,] = []'), SyntaxError, 'trailing elision');
assertThrowsInstanceOf(() => new Function('[a, ...b, c] = []'), SyntaxError, 'trailing param');
assertThrowsInstanceOf(() => new Function('[...[a]] = []'), SyntaxError, 'nested arraypattern');
assertThrowsInstanceOf(() => new Function('[...{a}] = []'), SyntaxError, 'nested objectpattern');
assertThrowsInstanceOf(() => new Function('[...a=b] = []'), SyntaxError, 'assignment expression');
assertThrowsInstanceOf(() => new Function('[...a()] = []'), SyntaxError, 'call expression');
assertThrowsInstanceOf(() => new Function('[...(a,b)] = []'), SyntaxError, 'comma expression');
assertThrowsInstanceOf(() => new Function('[...a++] = []'), SyntaxError, 'postfix expression');
assertThrowsInstanceOf(() => new Function('[...!a] = []'), SyntaxError, 'unary expression');
assertThrowsInstanceOf(() => new Function('[...a+b] = []'), SyntaxError, 'binary expression');
assertThrowsInstanceOf(() => new Function('var [...a.x] = []'), SyntaxError, 'lvalue expression in declaration');

// XXX: The way the current parser works, certain things, like a trailing comma
// and parenthesis, are lost before we check for destructuring.
// See bug 1041341. Once fixed, please update these assertions
assertThrowsInstanceOf(() =>
	assertThrowsInstanceOf(() => new Function('[...b,] = []'), SyntaxError)
	, Error);
assertThrowsInstanceOf(() =>
	assertThrowsInstanceOf(() => new Function('var [...(b)] = []'), SyntaxError)
	, Error);

var inputArray = [1, 2, 3];
var inputDeep = [1, inputArray];
var inputObject = {a: inputArray};
function *inputGenerator() {
  yield 1;
  yield 2;
  yield 3;
}

var o = {prop: null, call: function () { return o; }};
var globalEval = eval;

var expected = [2, 3];

function testAll(fn) {
  testDeclaration(fn);

  o.prop = null;
  assertEqArray(fn('[, ...(o.prop)]', inputArray, 'o.prop'), expected);
  o.prop = null;
  assertEqArray(fn('[, ...(o.call().prop)]', inputArray, 'o.prop'), expected);
}
function testDeclaration(fn) {
  assertEqArray(fn('[, ...rest]', inputArray), expected);
  assertEqArray(fn('[, ...rest]', inputGenerator()), expected);
  assertEqArray(fn('[, [, ...rest]]', inputDeep), expected);
  assertEqArray(fn('{a: [, ...rest]}', inputObject), expected);
}

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
    '(' + pattern + ') = input;' +
    'return ' + binding
  )(input);
}
testAll(testGlobal);

function testClosure(pattern, input, binding) {
  binding = binding || 'rest';
  return new Function('input',
    'var ' + binding + '; (function () {' +
    '(' + pattern + ') = input;' +
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
// XXX: ES6 requires the `Function` constructor to accept arbitrary
// `BindingElement`s as formal parameters. See Bug 1037939.
// Once fixed, please update the assertions below.
assertThrowsInstanceOf(() => testDeclaration(testArgumentFunction), SyntaxError);

function testThrow(pattern, input, binding) {
  binding = binding || 'rest';
  return new Function('input',
    'try { throw input }' +
    'catch(' + pattern + ') {' +
    'return ' + binding + '; }'
  )(input);
}
testDeclaration(testThrow);

// XXX: Support for let blocks and expressions will be removed in bug 1023609.
// However, they test a special code path in destructuring assignment so having
// these tests here for now seems like a good idea.
function testLetBlock(pattern, input, binding) {
  binding = binding || 'rest';
  return new Function('input',
    'let (' + pattern + ' = input)' +
    '{ return ' + binding + '; }'
  )(input);
}
testDeclaration(testLetBlock);

function testLetExpression(pattern, input, binding) {
  binding = binding || 'rest';
  return new Function('input',
    'return (let (' + pattern + ' = input) ' + binding + ');'
  )(input);
}
testDeclaration(testLetExpression);

