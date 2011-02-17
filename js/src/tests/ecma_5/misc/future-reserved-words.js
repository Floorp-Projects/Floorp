/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 497869;
var summary = "Implement FutureReservedWords per-spec";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var futureReservedWords =
  [
   "class",
   // "const", // Mozilla extension enabled even for versionless code
   "enum",
   "export",
   "extends",
   "import",
   "super",
  ];

var strictFutureReservedWords =
  [
   "implements",
   "interface",
   "let", // enabled: this file doesn't execute as JS1.7
   "package",
   "private",
   "protected",
   "public",
   "static",
   "yield", // enabled: this file doesn't execute as JS1.7
  ];

function testWord(word, expectNormal, expectStrict)
{
  var actual, status;

  // USE AS LHS FOR ASSIGNMENT

  if (expectNormal !== "skip")
  {
    actual = "";
    status = summary + ": " + word + ": normal assignment";
    try
    {
      eval(word + " = 'foo';");
      actual = "no error";
    }
    catch(e)
    {
      actual = e.name;
      status +=  ", " + e.name + ": " + e.message + " ";
    }
    reportCompare(expectNormal, actual, status);
  }

  actual = "";
  status = summary + ": " + word + ": strict assignment";
  try
  {
    eval("'use strict'; " + word + " = 'foo';");
    actual = "no error";
  }
  catch(e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  // USE IN VARIABLE DECLARATION

  if (expectNormal !== "skip")
  {
    actual = "";
    status = summary + ": " + word + ": normal var";
    try
    {
      eval("var " + word + ";");
      actual = "no error";
    }
    catch (e)
    {
      actual = e.name;
      status +=  ", " + e.name + ": " + e.message + " ";
    }
    reportCompare(expectNormal, actual, status);
  }

  actual = "";
  status = summary + ": " + word + ": strict var";
  try
  {
    eval("'use strict'; var " + word + ";");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  // USE IN FOR-IN VARIABLE DECLARATION

  if (expectNormal !== "skip")
  {
    actual = "";
    status = summary + ": " + word + ": normal for-in var";
    try
    {
      eval("for (var " + word + " in {});");
      actual = "no error";
    }
    catch (e)
    {
      actual = e.name;
      status +=  ", " + e.name + ": " + e.message + " ";
    }
    reportCompare(expectNormal, actual, status);
  }

  actual = "";
  status = summary + ": " + word + ": strict for-in var";
  try
  {
    eval("'use strict'; for (var " + word + " in {});");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  // USE AS CATCH IDENTIFIER

  if (expectNormal !== "skip")
  {
    actual = "";
    status = summary + ": " + word + ": normal var";
    try
    {
      eval("try { } catch (" + word + ") { }");
      actual = "no error";
    }
    catch (e)
    {
      actual = e.name;
      status +=  ", " + e.name + ": " + e.message + " ";
    }
    reportCompare(expectNormal, actual, status);
  }

  actual = "";
  status = summary + ": " + word + ": strict var";
  try
  {
    eval("'use strict'; try { } catch (" + word + ") { }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  // USE AS LABEL

  if (expectNormal !== "skip")
  {
    actual = "";
    status = summary + ": " + word + ": normal label";
    try
    {
      eval(word + ": while (false);");
      actual = "no error";
    }
    catch (e)
    {
      actual = e.name;
      status +=  ", " + e.name + ": " + e.message + " ";
    }
    reportCompare(expectNormal, actual, status);
  }

  actual = "";
  status = summary + ": " + word + ": strict label";
  try
  {
    eval("'use strict'; " + word + ": while (false);");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  // USE AS ARGUMENT NAME IN FUNCTION DECLARATION

  if (expectNormal !== "skip")
  {
    actual = "";
    status = summary + ": " + word + ": normal function argument";
    try
    {
      eval("function foo(" + word + ") { }");
      actual = "no error";
    }
    catch (e)
    {
      actual = e.name;
      status +=  ", " + e.name + ": " + e.message + " ";
    }
    reportCompare(expectNormal, actual, status);
  }

  actual = "";
  status = summary + ": " + word + ": strict function argument";
  try
  {
    eval("'use strict'; function foo(" + word + ") { }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  actual = "";
  status = summary + ": " + word + ": function argument retroactively strict";
  try
  {
    eval("function foo(" + word + ") { 'use strict'; }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  // USE AS ARGUMENT NAME IN FUNCTION EXPRESSION

  if (expectNormal !== "skip")
  {
    actual = "";
    status = summary + ": " + word + ": normal function expression argument";
    try
    {
      eval("var s = (function foo(" + word + ") { });");
      actual = "no error";
    }
    catch (e)
    {
      actual = e.name;
      status +=  ", " + e.name + ": " + e.message + " ";
    }
    reportCompare(expectNormal, actual, status);
  }

  actual = "";
  status = summary + ": " + word + ": strict function expression argument";
  try
  {
    eval("'use strict'; var s = (function foo(" + word + ") { });");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  actual = "";
  status = summary + ": " + word + ": function expression argument retroactively strict";
  try
  {
    eval("var s = (function foo(" + word + ") { 'use strict'; });");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  // USE AS ARGUMENT NAME WITH FUNCTION CONSTRUCTOR

  if (expectNormal !== "skip")
  {
    actual = "";
    status = summary + ": " + word + ": argument with normal Function";
    try
    {
      Function(word, "return 17");
      actual = "no error";
    }
    catch (e)
    {
      actual = e.name;
      status +=  ", " + e.name + ": " + e.message + " ";
    }
    reportCompare(expectNormal, actual, status);
  }

  actual = "";
  status = summary + ": " + word + ": argument with strict Function";
  try
  {
    Function(word, "'use strict'; return 17");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  // USE AS ARGUMENT NAME IN PROPERTY SETTER

  if (expectNormal !== "skip")
  {
    actual = "";
    status = summary + ": " + word + ": normal property setter argument";
    try
    {
      eval("var o = { set x(" + word + ") { } };");
      actual = "no error";
    }
    catch (e)
    {
      actual = e.name;
      status +=  ", " + e.name + ": " + e.message + " ";
    }
    reportCompare(expectNormal, actual, status);
  }

  actual = "";
  status = summary + ": " + word + ": strict property setter argument";
  try
  {
    eval("'use strict'; var o = { set x(" + word + ") { } };");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  actual = "";
  status = summary + ": " + word + ": property setter argument retroactively strict";
  try
  {
    eval("var o = { set x(" + word + ") { 'use strict'; } };");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  // USE AS FUNCTION NAME IN FUNCTION DECLARATION

  if (expectNormal !== "skip")
  {
    actual = "";
    status = summary + ": " + word + ": normal function name";
    try
    {
      eval("function " + word + "() { }");
      actual = "no error";
    }
    catch (e)
    {
      actual = e.name;
      status +=  ", " + e.name + ": " + e.message + " ";
    }
    reportCompare(expectNormal, actual, status);
  }

  actual = "";
  status = summary + ": " + word + ": strict function name";
  try
  {
    eval("'use strict'; function " + word + "() { }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  actual = "";
  status = summary + ": " + word + ": function name retroactively strict";
  try
  {
    eval("function " + word + "() { 'use strict'; }");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  // USE AS FUNCTION NAME IN FUNCTION EXPRESSION

  if (expectNormal !== "skip")
  {
    actual = "";
    status = summary + ": " + word + ": normal function expression name";
    try
    {
      eval("var s = (function " + word + "() { });");
      actual = "no error";
    }
    catch (e)
    {
      actual = e.name;
      status +=  ", " + e.name + ": " + e.message + " ";
    }
    reportCompare(expectNormal, actual, status);
  }

  actual = "";
  status = summary + ": " + word + ": strict function expression name";
  try
  {
    eval("'use strict'; var s = (function " + word + "() { });");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);

  actual = "";
  status = summary + ": " + word + ": function expression name retroactively strict";
  try
  {
    eval("var s = (function " + word + "() { 'use strict'; });");
    actual = "no error";
  }
  catch (e)
  {
    actual = e.name;
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);
}

function testFutureReservedWord(word)
{
  /*
   * NON-STANDARD DEVIATION: At one point in history SpiderMonkey unreserved
   * most of the reserved words in ES3, including those words which are
   * FutureReservedWords in ES5.  It's too late this release cycle to expect
   * "SyntaxError" for the normal code cases, so for now we "skip" testing
   * these words in normal code.  (We don't test for "no error" because that
   * would be contrary to the spec, and this test is not in an extensions/
   * directory.)
   */
  testWord(word, "skip", "SyntaxError");
}

function testStrictFutureReservedWord(word)
{
  testWord(word, "no error", "SyntaxError");
}

futureReservedWords.forEach(testFutureReservedWord);
strictFutureReservedWords.forEach(testStrictFutureReservedWord);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
