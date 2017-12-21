/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that Reflect and acorn create the same AST for ES5.
 */

const acorn = require("acorn/acorn");
const { Reflect } = require("resource://gre/modules/reflect.jsm");

const testCode = "" + function main () {
  function makeAcc(n) {
    return function () {
      return ++n;
    };
  }

  var acc = makeAcc(10);

  for (var i = 0; i < 10; i++) {
    acc();
  }

  console.log(acc());
};

function run_test() {
  const reflectAST = Reflect.parse(testCode);
  const acornAST = acorn.parse(testCode);

  info("Reflect AST:");
  info(JSON.stringify(reflectAST, null, 2));
  info("acorn AST:");
  info(JSON.stringify(acornAST, null, 2));

  checkEquivalentASTs(reflectAST, acornAST);
}
