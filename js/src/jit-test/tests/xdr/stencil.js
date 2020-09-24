
load(libdir + "asserts.js");

/*
 * This exercises stencil XDR encoding and decoding using a broad
 * smoke testing.
 *
 * A set of scripts exercising various codepaths are XDR-encoded,
 * then decoded, and then executed.  Their output is compared to
 * the execution of the scripts through a normal path and the
 * outputs checked.
 */

/*
 * Exercises global scope access and object literals, as well as some
 * simple object destructuring.
 */
const testGlobal0 = 13;
let testGlobal1 = undefined;
var testGlobal2 = undefined;

const SCRIPT_0 = `
testGlobal1 = 123456789012345678901234567890n;
testGlobal2 = {'foo':3, 'bar': [1, 2, 3],
                   '4': 'zing'};
var testGlobal3 = /NewlyDefinedGlobal/;
function testGlobal4(a, {b}) {
    return a + b.foo + b['bar'].reduce((a,b) => (a+b), 0);
               + b[4].length +
               + testGlobal3.toString().length;
};
testGlobal4(Number(testGlobal1), {b:testGlobal2})
`;

/*
 * Exercises function scopes, lexical scopes, var and let
 * within them, and some longer identifiers, and array destructuring in
 * arguments.  Also contains some tiny atoms and globls access.
 */
const SCRIPT_1 = `
function foo(a, b, c) {
  var q = a * (b + c);
  let bix = function (d, e) {
    let x = a + d;
    var y = e * b;
    const a0 = q + x + y;
    for (let i = 0; i < 3; i++) {
      y = a0 + Math.PI + y;
    }
    return y;
  };
  function bang(d, [e, f]) {
    let reallyLongIdentifierName = a + d;
    var y = e * b;
    const z = reallyLongIdentifierName + f;
    return z;
  }
  return bix(1, 2) + bang(3, [4, 5, 6]);
}
foo(1, 2, 3)
`;

/*
 * Exercises eval and with scopes, object destructuring, function rest
 * arguments.
 */
const SCRIPT_2 = `
function foo(with_obj, ...xs) {
  const [x0, x1, ...xrest] = xs;
  eval('var x2 = x0 + x1');
  var sum = [];
  with (with_obj) {
    sum.push(x2 + xrest.length);
  }
  sum.push(x2 + xrest.length);
  return sum;
}
foo({x2: 99}, 1, 2, 3, 4, 5, 6)
`;

function test_script(script_str) {
  const eval_f = eval;
  const bytes = compileStencilXDR(script_str);
  const result = evalStencilXDR(bytes);
  assertDeepEq(result, eval_f(script_str));
}

function tests() {
  test_script(SCRIPT_0);
  test_script(SCRIPT_1);
  test_script(SCRIPT_2);
}

tests()
