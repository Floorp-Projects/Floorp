// |jit-test| skip-if: !('disassemble' in this)
// Strict direct eval supports static binding of identifiers.

"use strict";

// Check that a script contains a particular bytecode sequence.
//
// `actual` is the output of the `disassemble()` shell builtin.
// `expected` is a semicolon-separated string of opcodes.
//    Can include regular expression syntax, e.g. "GetLocal .* x$"
//    to match a GetLocal instruction with ` x` at the end of the line.
// `message` is a string to include in the error message if the test fails.
//
function assertBytecode(actual, expected, message) {
  // Grab the opcode name and everything after to the end of the line.  This
  // intentionally includes the expression stack, as that is what makes the
  // `GetLocal .* y$` trick work. The disassemble() output is like this:
  //
  //     00016:  10 GetLocal 0                      # x y
  //
  let actualOps =
      actual.split('\n')
      .map(s => /^\d{5}: +\d+ +(.*)$/.exec(s)?.[1])
      .filter(x => x !== undefined);

  // Turn the expectations into regular expressions.
  let expectedOps =
      expected.split(';')
      .map(s => {
        s = s.trim();
        // If the op is a single word, like `Dup`, add `\b` to rule out
        // similarly named ops like `Dup2`.
        if (/^\w+$/.test(s)) {
          s += "\\b";
        }
        return new RegExp("^" + s);
      });

  // The condition on this for-loop is saying, "continue as long as the range
  // [i..i+expectedOps.length] is entirely within in the actualOps array".
  // Hence the rare use of `<=` in a for-loop!
  for (let i = 0; i + expectedOps.length <= actualOps.length; i++) {
    if (expectedOps.every((expectRegExp, j) => expectRegExp.test(actualOps[i + j]))) {
      // Found a complete match.
      return;
    }
  }
  throw new Error(`Assertion failed: ${message}\nexpected ${uneval(expected)}, got:\n${actual}`);
}


// --- Tests

var bytecode;

// `var`s in strict eval code are statically bound as locals.
eval(`
  var pet = "ostrich";
  bytecode = disassemble();
  pet
`);
assertEq(globalThis.hasOwnProperty('pet'), false);
assertBytecode(bytecode, 'String "ostrich"; SetLocal; Pop',
               "`pet` is stored in a stack local");
assertBytecode(bytecode, "GetLocal; SetRval; RetRval",
               "`pet` is loaded from the local at the end of the eval code");

// Same for top-level `function`s.
eval(`
  function banana() { return "potassium"; }
  bytecode = disassemble();
`);
assertEq(globalThis.hasOwnProperty('banana'), false);
assertBytecode(bytecode, 'Lambda .* banana; SetLocal; Pop',
               "`banana` is stored in a stack local");

// Same for let/const.
eval(`
  let a = "ushiko-san";
  const b = "umao-san";
  bytecode = disassemble();
  [a, b]
`);
assertBytecode(bytecode, 'String "ushiko-san"; InitLexical; Pop',
               "`let a` is stored in a stack local");
assertBytecode(bytecode, 'String "umao-san"; InitLexical; Pop',
               "`const b` is stored in a stack local");
assertBytecode(bytecode, 'GetLocal .* a$; InitElemArray; GetLocal .* b$; InitElemArray',
               "lexical variables are loaded from stack locals");

// Same for arguments and locals in functions declared in strict eval code.
let g = eval(`
  function f(a) {
    let x = 'x';
    function g(b) {
      let y = "wye";
      return [f, a, x, g, b, y];
    }
    return g;
  }
  f();
`);
bytecode = disassemble(g);
assertBytecode(bytecode, 'GetAliasedVar "f"',
               "closed-over eval-scope `function` is accessed via aliased op");
assertBytecode(bytecode, 'GetAliasedVar "a"',
               "closed-over argument is accessed via aliased op");
assertBytecode(bytecode, 'GetAliasedVar "x"',
               "closed-over local `let` variable is accessed via aliased op");
assertBytecode(bytecode, 'GetAliasedVar "g"',
               "closed-over local `function` is accessed via aliased op");
assertBytecode(bytecode, 'GetArg .* b$',
               "non-closed-over arguments are optimized");
assertBytecode(bytecode, 'GetLocal .* y$',
               "non-closed-over locals are optimized");

// Closed-over bindings declared in strict eval code are statically bound.
var fac = eval(`
  bytecode = disassemble();
  function fac(x) { return x <= 1 ? 1 : x * fac(x - 1); }
  fac
`);
assertBytecode(bytecode, 'SetAliasedVar "fac"',
               "strict eval code accesses closed-over top-level function using aliased ops");
assertBytecode(disassemble(fac), 'GetAliasedVar "fac"',
               "function in strict eval accesses itself using aliased ops");

// References to `this` in an enclosing method are statically bound.
let obj = {
  m(s) { return eval(s); }
};
let result = obj.m(`
  bytecode = disassemble();
  this;
`);
assertEq(result, obj);
assertBytecode(bytecode, 'GetAliasedVar ".this"',
               "strict eval in a method can access `this` using aliased ops");

// Same for `arguments`.
function fn_with_args() {
  return eval(`
    bytecode = disassemble();
    arguments[0];
  `);
}
assertEq(fn_with_args(117), 117);
assertBytecode(bytecode, 'GetAliasedVar "arguments"',
               "strict eval in a function can access `arguments` using aliased ops");

// The frontend can emit GName ops in strict eval.
result = eval(`
  bytecode = disassemble();
  fn_with_args;
`);
assertEq(result, fn_with_args);
assertBytecode(bytecode, 'GetGName "fn_with_args"',
               "strict eval code can optimize access to globals");

// Even within a function.
function test_globals_in_function() {
  result = eval(`
    bytecode = disassemble();
    fn_with_args;
  `);
  assertEq(result, fn_with_args);
  assertBytecode(bytecode, 'GetGName "fn_with_args"',
                 "strict eval code in a function can optimize access to globals");
}
test_globals_in_function();

// Nested eval is no obstacle.
{
  let outer = "outer";
  const f = function (code, a, b) {
    return eval(code);
  };
  let result = f(`
    eval("bytecode = disassemble();\\n" +
         "outer += a + b;\\n");
  `, 3, 4);
  assertEq(outer, "outer7");
  assertBytecode(bytecode, 'GetAliasedVar "outer"',
                 "access to outer bindings is optimized even through nested strict evals");
  assertBytecode(bytecode, 'GetAliasedVar "a"',
                 "access to outer bindings is optimized even through nested strict evals");
  assertBytecode(bytecode, 'SetAliasedVar "outer"',
                 "assignment to outer bindings is optimized even through nested strict evals");
}

// Assignment to an outer const is handled correctly.
{
  const doNotSetMe = "i already have a value, thx";
  let f = eval(`() => { doNotSetMe = 34; }`);
  assertBytecode(disassemble(f), 'ThrowSetConst "doNotSetMe"',
                 "assignment to outer const in strict eval code emits ThrowSetConst");
}

// OK, there are other scopes but let's just do one more: the
// computed-property-name scope.
{
  let stashed;
  (class C {
    [(
      eval(`
        var secret = () => C;
        stashed = () => secret;
      `),
      "method"
    )]() {
      return "ok";
    }
  });

  bytecode = disassemble(stashed());
  assertBytecode(bytecode, 'GetAliasedVar "C"',
                 "access to class name uses aliased ops");
  let C = stashed()();
  assertEq(new C().method(), "ok");
}

