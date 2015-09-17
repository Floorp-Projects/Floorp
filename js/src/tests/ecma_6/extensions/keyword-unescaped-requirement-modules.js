/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1204027;
var summary =
  "Escape sequences aren't allowed in bolded grammar tokens (that is, in " +
  "keywords, possibly contextual keywords)";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var badModules =
  [
   "\\u0069mport f from 'g'",
   "i\\u006dport g from 'h'",
   "import * \\u0061s foo",
   "import {} fro\\u006d 'bar'",
   "import { x \\u0061s y } from 'baz'",

   "\\u0065xport function f() {}",
   "e\\u0078port function g() {}",
   "export * fro\\u006d 'fnord'",
   "export d\\u0065fault var x = 3;",
   "export { q } fro\\u006d 'qSupplier';",

  ];

if (typeof parseModule === "function")
{
  for (var module of badModules)
  {
    assertThrowsInstanceOf(() => parseModule(module), SyntaxError,
                           "bad behavior for: " + module);
  }
}

if (typeof Reflect.parse === "function")
{
  var twoStatementAST =
    Reflect.parse(`export { x } /* ASI should trigger here */
                  fro\\u006D`,
                  { target: "module" });

  var statements = twoStatementAST.body;
  assertEq(statements.length, 2,
           "should have two items in the module, not one ExportDeclaration");
  assertEq(statements[0].type, "ExportDeclaration");
  assertEq(statements[1].type, "ExpressionStatement");
  assertEq(statements[1].expression.name, "from");

  var oneStatementAST =
    Reflect.parse(`export { x } /* no ASI here */
                  from 'foo'`,
                  { target: "module" });

  assertEq(oneStatementAST.body.length, 1);
  assertEq(oneStatementAST.body[0].type, "ExportDeclaration");
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
