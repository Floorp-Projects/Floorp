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

  // USE IN VARIABLE DECLARATION

  actual = "";
  status = summary + ", normal var: " + word;
  try
  {
    eval("var " + word + ";");
    actual = "no error";
  }
  catch (e)
  {
    actual = "error";
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectNormal, actual, status);

  actual = "";
  status = summary + ", strict var: " + word;
  try
  {
    eval("'use strict'; var " + word + ";");
    actual = "no error";
  }
  catch (e)
  {
    actual = "error";
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);


  // USE AS LHS FOR ASSIGNMENT

  actual = "";
  status = summary + ", normal assignment: " + word;
  try
  {
    eval(word + " = 'foo';");
    actual = "no error";
  }
  catch(e)
  {
    actual = "error";
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectNormal, actual, status);

  actual = "";
  status = summary + ", strict assignment: " + word;
  try
  {
    eval("'use strict'; " + word + " = 'foo';");
    actual = "no error";
  }
  catch(e)
  {
    actual = "error";
    status +=  ", " + e.name + ": " + e.message + " ";
  }
  reportCompare(expectStrict, actual, status);
}

function testFutureReservedWord(word)
{
  testWord(word, "error", "error");
}

function testStrictFutureReservedWord(word)
{
  testWord(word, "no error", "error");
}

futureReservedWords.forEach(testFutureReservedWord);
strictFutureReservedWords.forEach(testStrictFutureReservedWord);

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("All tests passed!");
