/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that acorn's lenient parser gives something usable.
 */

const acorn_loose = require("acorn/acorn_loose");

function run_test() {
  let actualAST = acorn_loose.parse_dammit("let x = 10", {});

  do_print("Actual AST:");
  do_print(JSON.stringify(actualAST, null, 2));
  do_print("Expected AST:");
  do_print(JSON.stringify(expectedAST, null, 2));

  checkEquivalentASTs(expectedAST, actualAST);
}

const expectedAST = {
  "type": "Program",
  "start": 0,
  "end": 10,
  "body": [
    {
      "type": "ExpressionStatement",
      "start": 0,
      "end": 3,
      "expression": {
        "type": "Identifier",
        "start": 0,
        "end": 3,
        "name": "let"
      }
    },
    {
      "type": "ExpressionStatement",
      "start": 4,
      "end": 10,
      "expression": {
        "type": "AssignmentExpression",
        "start": 4,
        "end": 10,
        "operator": "=",
        "left": {
          "type": "Identifier",
          "start": 4,
          "end": 5,
          "name": "x"
        },
        "right": {
          "type": "Literal",
          "start": 8,
          "end": 10,
          "value": 10,
          "raw": "10"
        }
      }
    }
  ]
};
