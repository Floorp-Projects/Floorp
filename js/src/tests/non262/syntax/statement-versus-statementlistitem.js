// |reftest| skip-if(!xulRuntime.shell) -- needs evaluate, parseModule
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1288459;
var summary =
  "Properly implement the spec's distinctions between StatementListItem and " +
  "Statement grammar productions and their uses";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var counter = 0;

// Test all the different contexts that expect a Statement.

function contextAcceptsStatement(pat)
{
  var goodCode =
`function f${counter++}()
{
  ${pat.replace("@@@", "let \n {} \n ;")}
}
`;

  evaluate(goodCode);

  var badCode =
`function f${counter++}()
{
  ${pat.replace("@@@", "let {} \n ;")}
}
`;

  try
  {
    evaluate(badCode);
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assertEq(e instanceof SyntaxError, true,
             "didn't get a syntax error, instead got: " + e);
  }
}

contextAcceptsStatement("if (0) @@@");
contextAcceptsStatement("if (0) ; else @@@");
contextAcceptsStatement("if (0) ; else if (0) @@@");
contextAcceptsStatement("if (0) ; else if (0) ; else @@@");
// Can't test do-while loops this way because the Statement isn't in trailing
// position, so 'let' followed by newline *must* be followed by 'while'.
contextAcceptsStatement("while (1) @@@");
contextAcceptsStatement("for (1; 1; 0) @@@");
contextAcceptsStatement("for (var x; 1; 0) @@@");
contextAcceptsStatement("for (let x; 1; 0) @@@");
contextAcceptsStatement("for (const x = 1; 1; 0) @@@");
contextAcceptsStatement("for (x in 0) @@@");
contextAcceptsStatement("for (var x in 0) @@@");
contextAcceptsStatement("for (let x in 0) @@@");
contextAcceptsStatement("for (const x in 0) @@@");
contextAcceptsStatement("for (x of []) @@@");
contextAcceptsStatement("for (var x of []) @@@");
contextAcceptsStatement("for (let x of []) @@@");
contextAcceptsStatement("for (const x of []) @@@");
contextAcceptsStatement("with (1) @@@");
contextAcceptsStatement("foo: @@@");

// Test all the different contexts that expect a StatementListItem.

function contextAcceptsStatementListItem(pat)
{
  var goodCode =
`function f${counter++}()
{
  ${pat.replace("@@@", "let \n [] = [] ;")}
}
`;

  evaluate(goodCode);

  var moarGoodCode = pat.replace("@@@", "let \n [] = [] ;");
  evaluate(moarGoodCode);

  var goodModuleCode = pat.replace("@@@", "let \n [] = [] ;");
  parseModule(goodModuleCode);

  var badCode =
`function f${counter++}()
{
  ${pat.replace("@@@", "let \n let = 3 ;")}
}
`;

  try
  {
    evaluate(badCode);
    throw new Error("didn't throw");
  }
  catch (e)
  {
    assertEq(e instanceof SyntaxError, true,
             "didn't get a syntax error, instead got: " + e);
  }
}

contextAcceptsStatementListItem("{ @@@ }");
contextAcceptsStatementListItem("switch (1) { case 1: @@@ }");
contextAcceptsStatementListItem("switch (1) { default: @@@ }");
contextAcceptsStatementListItem("switch (1) { case 0: case 1: @@@ }");
contextAcceptsStatementListItem("switch (1) { case 0: break; case 1: @@@ }");
contextAcceptsStatementListItem("switch (1) { default: @@@ case 2: }");
contextAcceptsStatementListItem("try { @@@ } catch (e) { }");
contextAcceptsStatementListItem("try { @@@ } finally { }");
contextAcceptsStatementListItem("try { @@@ } catch (e) { } finally { }");
contextAcceptsStatementListItem("try { } catch (e) { @@@ }");
contextAcceptsStatementListItem("try { } finally { @@@ }");
contextAcceptsStatementListItem("try { } catch (e) { @@@ } finally { }");
contextAcceptsStatementListItem("try { } catch (e) { } finally { @@@ }");
contextAcceptsStatementListItem("_ => { @@@ }");
contextAcceptsStatementListItem("function q() { @@@ }");
contextAcceptsStatementListItem("function* q() { @@@ }");
contextAcceptsStatementListItem("(function() { @@@ })");
contextAcceptsStatementListItem("(function*() { @@@ })");
contextAcceptsStatementListItem("({ *q() { @@@ } })");
contextAcceptsStatementListItem("({ q() { @@@ } })");
contextAcceptsStatementListItem("({ get q() { @@@ } })");
contextAcceptsStatementListItem("({ set q(v) { @@@ } })");
contextAcceptsStatementListItem("(class { f() { @@@ } })");
contextAcceptsStatementListItem("(class { static f() { @@@ } })");
contextAcceptsStatementListItem("(class { static *f() { @@@ } })");
contextAcceptsStatementListItem("(class { static get f() { @@@ } })");
contextAcceptsStatementListItem("(class { static set f(v) { @@@ } })");
contextAcceptsStatementListItem("(class { static() { @@@ } })");
contextAcceptsStatementListItem("@@@");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
