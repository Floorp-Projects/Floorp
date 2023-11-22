// |reftest| skip-if(!this.hasOwnProperty('Reflect')||!Reflect.parse) -- uses Reflect.parse(..., { loc: true}) to trigger the column-computing API
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

//-----------------------------------------------------------------------------
var BUGNUMBER = 1551916;
var summary =
  "Optimize computing a column number as count of code points by caching " +
  "column numbers (and whether each chunk might contain anything multi-unit) " +
  "and counting forward from them";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

// Various testing of column-number computations, with respect to counting as
// code points or units, for very long lines.
//
// This test should pass when column numbers are counts of code points (current
// behavior) or code units (past behavior).  It also *should* pass for any valid
// |TokenStreamAnyChars::ColumnChunkLength| value (it must be at least 4 so that
// the maximum-length code point in UTF-8/16 will fit in a single chunk),
// because the value of that constant should be externally invisible save for
// perf effects. (As a result, recompiling and running this test with a variety
// of different values assigned to that constant is a good smoke-test of column
// number computation, that doesn't require having to closely inspect any
// column-related code.)
//
// However, this test is structured on the assumption that that constant has the
// value 128, in order to exercise in targeted fashion various column number
// computation edge cases.
//
// All this testing *could* be written to not be done with |Reflect.parse| --
// backwards column computations happen even when compiling normal code, in some
// cases.  But it's much more the exception than the rule.  And |Reflect.parse|
// has *very* predictable column-computation operations (basically start/end
// coordinates are computed immediately when the end of an AST node is reached)
// that make it easier to recognize what the exact pattern of computations for
// which offsets will look like.

// Helper function for checking node location tersely.
function checkLoc(node, expectedStart, expectedEnd)
{
  let start = node.loc.start;

  assertEq(start.line, expectedStart[0],
           "start line number must be as expected");
  assertEq(start.column, expectedStart[1],
           "start column number must be as expected");

  let end = node.loc.end;

  assertEq(end.line, expectedEnd[0], "end line number must be as expected");
  assertEq(end.column, expectedEnd[1],
           "end column number must be as expected");
}

function lengthInCodePoints(str)
{
  return [...str].length;
}

// True if column numbers are counts of code points, false otherwise.  This
// constant can be used to short-circuit testing that isn't point/unit-agnostic.
const columnsAreCodePoints = (function()
{
  var columnTypes = [];

  function checkColumn(actual, expectedPoints, expectedUnits)
  {
    if (actual === expectedPoints)
      columnTypes.push("p");
    else if (actual === expectedUnits)
      columnTypes.push("u");
    else
      columnTypes.push("x");
  }

  var script = Reflect.parse('"😱😱😱😱";', { loc: true });
  assertEq(script.type, "Program");
  assertEq(script.loc.start.line, 1);
  assertEq(script.loc.end.line, 1);
  assertEq(script.loc.start.column, 1);
  checkColumn(script.loc.end.column, 8, 12);

  var body = script.body;
  assertEq(body.length, 1);

  var stmt = body[0];
  assertEq(stmt.type, "ExpressionStatement");
  assertEq(stmt.loc.start.line, 1);
  assertEq(stmt.loc.end.line, 1);
  assertEq(stmt.loc.start.column, 1);
  checkColumn(stmt.loc.end.column, 8, 12);

  var expr = stmt.expression;
  assertEq(expr.type, "Literal");
  assertEq(expr.value, "😱😱😱😱");
  assertEq(expr.loc.start.line, 1);
  assertEq(expr.loc.end.line, 1);
  assertEq(expr.loc.start.column, 1);
  checkColumn(expr.loc.end.column, 7, 11);

  var checkResult = columnTypes.join(",");

  assertEq(checkResult === "p,p,p" || checkResult === "u,u,u", true,
           "columns must be wholly code points or units: " + checkResult);

  return checkResult === "p,p,p";
})();

// Start with some basic sanity-testing, without regard to exactly when, how, or
// in what order (offset => column) computations are performed.
function testSimple()
{
  if (!columnsAreCodePoints)
    return;

  // Array elements within the full |simpleCode| string constructed below are
  // one-element arrays containing the string "😱😱#x" where "#" is the
  // character that, in C++, could be written as |'(' + i| where |i| is the
  // index of the array within the outer array.
  let simpleCodeArray =
    [
      'var Q = [[',  // column 1, offset 0
      // REPEAT
      '"😱😱(x"],["',  // column 11, offset 10
      '😱😱)x"],["😱',  // column 21, offset 22
      '😱*x"],["😱😱',  // column 31, offset 35
      '+x"],["😱😱,',  // column 41, offset 48
      'x"],["😱😱-x',  // column 51, offset 60
      '"],["😱😱.x"',  // column 61, offset 72
      '],["😱😱/x"]',  // column 71, offset 84
      ',["😱😱0x"],',  // column 81, offset 96
      '["😱😱1x"],[',  // column 91, offset 108
      // REPEAT
      '"😱😱2x"],["',  // column 101, offset 120 -- chunk limit between "]
      '😱😱3x"],["😱',  // column 111, offset 132
      '😱4x"],["😱😱',  // column 121, offset 145
      '5x"],["😱😱6',  // column 131, offset 158
      'x"],["😱😱7x',  // column 141, offset 170
      '"],["😱😱8x"',  // column 151, offset 182
      '],["😱😱9x"]',  // column 161, offset 194
      ',["😱😱:x"],',  // column 171, offset 206
      '["😱😱;x"],[',  // column 181, offset 218
      // REPEAT
      '"😱😱<x"],["',  // column 191, offset 230
      '😱😱=x"],["😱',  // column 201, offset 242
      '😱>x"],["😱😱',  // column 211, offset 255 -- chunk limit splits first 😱
      '?x"],["😱😱@',  // column 221, offset 268
      'x"],["😱😱Ax',  // column 231, offset 280
      '"],["😱😱Bx"',  // column 241, offset 292
      '],["😱😱Cx"]',  // column 251, offset 304
      ',["😱😱Dx"],',  // column 261, offset 316
      '["😱😱Ex"],[',  // column 271, offset 328
      // REPEAT
      '"😱😱Fx"],["',  // column 281, offset 340
      '😱😱Gx"],["😱',  // column 291, offset 352
      '😱Hx"],["😱😱',  // column 301, offset 365
      'Ix"],["😱😱J',  // column 311, offset 378 -- chunk limit between ["
      'x"],["😱😱Kx',  // column 321, offset 390
      '"],["😱😱Lx"',  // column 331, offset 402
      '],["😱😱Mx"]',  // column 341, offset 414
      ',["😱😱Nx"],',  // column 351, offset 426
      '["😱😱Ox"]];',  // column 361 (10 long), offset 438 (+12 to end)
    ];
  let simpleCode = simpleCodeArray.join("");

  // |simpleCode| overall contains this many code points.  (This number is
  // chosen to be several |TokenStreamAnyChars::ColumnChunkLength = 128| chunks
  // long so that long-line handling is exercised, and the relevant vector
  // increased in length, for more than one chunk [which would be too short to
  // trigger chunking] and for more than two chunks [so that vector extension
  // will eventually occur].)
  const CodePointLength = 370;

  assertEq(lengthInCodePoints(simpleCode), CodePointLength,
           "code point count should be correct");

  // |simpleCodeArray| contains this many REPEAT-delimited cycles.
  const RepetitionNumber = 4;

  // Each cycle consists of this many elements.
  const ElementsPerCycle = 9;

  // Each element in a cycle has at least this many 😱.
  const MinFaceScreamingPerElementInCycle = 2;

  // Each cycle consists of many elements with three 😱.
  const ElementsInCycleWithThreeFaceScreaming = 2;

  // Compute the overall number of UTF-16 code units.  (UTF-16 because this is a
  // JS string as input.)
  const OverallCodeUnitCount =
    CodePointLength +
    RepetitionNumber * (ElementsPerCycle * MinFaceScreamingPerElementInCycle +
                        ElementsInCycleWithThreeFaceScreaming);

  // Code units != code points.
  assertEq(OverallCodeUnitCount > CodePointLength, true,
           "string contains code points outside BMP, so length in units " +
           "exceeds length in points");

  // The overall computed number of code units has this exact numeric value.
  assertEq(OverallCodeUnitCount, 450,
           "code unit count computation produces this value");

  // The overall computed number of code units matches the string length.
  assertEq(simpleCode.length, OverallCodeUnitCount, "string length must match");

  // Evaluate the string.
  var Q;
  eval(simpleCode);

  // Verify characteristics of the resulting execution.
  assertEq(Array.isArray(Q), true);

  const NumArrayElements = 40;
  assertEq(Q.length, NumArrayElements);
  Q.forEach((v, i) => {
    assertEq(Array.isArray(v), true);
    assertEq(v.length, 1);
    assertEq(v[0], "😱😱" + String.fromCharCode('('.charCodeAt(0) + i) + "x");
  });

  let parseTree = Reflect.parse(simpleCode, { loc: true });

  // Check the overall script.
  assertEq(parseTree.type, "Program");
  checkLoc(parseTree, [1, 0], [1, 370]);
  assertEq(parseTree.body.length, 1);

  // Check the coordinates of the declaration.
  let varDecl = parseTree.body[0];
  assertEq(varDecl.type, "VariableDeclaration");
  checkLoc(varDecl, [1, 0], [1, 369]);

  // ...and its initializing expression.
  let varInit = varDecl.declarations[0].init;
  assertEq(varInit.type, "ArrayExpression");
  checkLoc(varInit, [1, 8], [1, 369]);

  // ...and then every literal inside it.
  assertEq(varInit.elements.length, NumArrayElements, "array literal length");

  const ItemLength = lengthInCodePoints('["😱😱#x"],');
  assertEq(ItemLength, 9, "item length check");

  for (let i = 0; i < NumArrayElements; i++)
  {
    let elem = varInit.elements[i];
    assertEq(elem.type, "ArrayExpression");

    let startCol = 9 + i * ItemLength;
    let endCol = startCol + ItemLength - 1;
    checkLoc(elem, [1, startCol], [1, endCol]);

    let arrayElems = elem.elements;
    assertEq(arrayElems.length, 1);

    let str = arrayElems[0];
    assertEq(str.type, "Literal");
    assertEq(str.value,
             "😱😱" + String.fromCharCode('('.charCodeAt(0) + i) + "x");
    checkLoc(str, [1, startCol + 1], [1, endCol - 1]);
  }
}
testSimple();

// Test |ChunkInfo::unitsType() == UnitsType::GuaranteedSingleUnit| -- not that
// it should be observable, precisely, but effects of mis-applying or
// miscomputing it would in principle be observable if such were happening.
// This test also is intended to to be useful for (manually, in a debugger)
// verifying that the optimization is computed and kicks in correctly.
function testGuaranteedSingleUnit()
{
  if (!columnsAreCodePoints)
    return;

  // Begin a few array literals in a first chunk to test column computation in
  // that first chunk.
  //
  // End some of them in the first chunk to test columns *before* we know we
  // have a long line.
  //
  // End one array *outside* the first chunk to test a computation inside a
  // first chunk *after* we know we have a long line and have computed a first
  // chunk.
  let mixedChunksCode = "var Z = [ [ [],"; // column 1, offset 0
  assertEq(mixedChunksCode.length, 15);
  assertEq(lengthInCodePoints(mixedChunksCode), 15);

  mixedChunksCode +=
    " ".repeat(128 - mixedChunksCode.length); // column 16, offset 15
  assertEq(mixedChunksCode.length, 128);
  assertEq(lengthInCodePoints(mixedChunksCode), 128);

  // Fill out a second chunk as also single-unit, with an outer array literal
  // that begins in this chunk but finishes in the next (to test column
  // computation in a prior, guaranteed-single-unit chunk).
  mixedChunksCode += "[" + "[],".repeat(42) + " "; // column 129, offset 128
  assertEq(mixedChunksCode.length, 256);
  assertEq(lengthInCodePoints(mixedChunksCode), 256);

  // Add a third chunk with one last empty nested array literal (so that we
  // tack on another chunk, and conclude the second chunk is single-unit, before
  // closing the enclosing array literal).  Then close the enclosing array
  // literal.  Finally start a new string literal element containing
  // multi-unit code points.  For good measure, make the chunk *end* in the
  // middle of such a code point, so that the relevant chunk limit must be
  // retracted one code unit.
  mixedChunksCode += "[] ], '" + "😱".repeat(61); // column 257, offset 256
  assertEq(mixedChunksCode.length, 384 + 1);
  assertEq(lengthInCodePoints(mixedChunksCode), 324);

  // Wrap things up.  Terminate the string, then terminate the nested array
  // literal to trigger a column computation within the first chunk that can
  // benefit from knowing the first chunk is all single-unit.  Next add a *new*
  // element to the outermost array, a string literal that contains a line
  // terminator.  The terminator invalidates the column computation cache, so
  // when the outermost array is closed, location info for it will not hit the
  // cache.  Finally, tack on the terminating semicolon for good measure.
  mixedChunksCode += "' ], '\u2028' ];"; // column 325, offset 385
  assertEq(mixedChunksCode.length, 396);
  assertEq(lengthInCodePoints(mixedChunksCode), 335);

  let parseTree = Reflect.parse(mixedChunksCode, { loc: true });

  // Check the overall script.
  assertEq(parseTree.type, "Program");
  checkLoc(parseTree, [1, 0], [2, 4]);
  assertEq(parseTree.body.length, 1);

  // Check the coordinates of the declaration.
  let varDecl = parseTree.body[0];
  assertEq(varDecl.type, "VariableDeclaration");
  checkLoc(varDecl, [1, 0], [2, 3]);

  // ...and its initializing expression.
  let varInit = varDecl.declarations[0].init;
  assertEq(varInit.type, "ArrayExpression");
  checkLoc(varInit, [1, 8], [2, 3]);

  let outerArrayElements = varInit.elements;
  assertEq(outerArrayElements.length, 2);

  {
    // Next the first element, the array inside the initializing expression.
    let nestedArray = varInit.elements[0];
    assertEq(nestedArray.type, "ArrayExpression");
    checkLoc(nestedArray, [1, 10], [1, 327]);

    // Now inside that nested array.
    let nestedArrayElements = nestedArray.elements;
    assertEq(nestedArrayElements.length, 3);

    // First the [] in chunk #0
    let emptyArray = nestedArrayElements[0];
    assertEq(emptyArray.type, "ArrayExpression");
    assertEq(emptyArray.elements.length, 0);
    checkLoc(emptyArray, [1, 12], [1, 14]);

    // Then the big array of empty arrays starting in chunk #1 and ending just
    // barely in chunk #2.
    let bigArrayOfEmpties = nestedArrayElements[1];
    assertEq(bigArrayOfEmpties.type, "ArrayExpression");
    assertEq(bigArrayOfEmpties.elements.length, 42 + 1);
    bigArrayOfEmpties.elements.forEach((elem, i) => {
      assertEq(elem.type, "ArrayExpression");
      assertEq(elem.elements.length, 0);
      if (i !== 42)
        checkLoc(elem, [1, 129 + i * 3], [1, 131 + i * 3]);
      else
        checkLoc(elem, [1, 256], [1, 258]); // last element was hand-placed
    });

    // Then the string literal of multi-unit code points beginning in chunk #2
    // and ending just into chunk #3 on a second line.
    let multiUnitStringLiteral = nestedArrayElements[2];
    assertEq(multiUnitStringLiteral.type, "Literal");
    assertEq(multiUnitStringLiteral.value, "😱".repeat(61));
    checkLoc(multiUnitStringLiteral, [1, 262], [1, 325]);
  }

  {
    // Finally, the string literal containing a line terminator as element in
    // the outermost array.
    let stringLiteralWithEmbeddedTerminator = outerArrayElements[1];
    assertEq(stringLiteralWithEmbeddedTerminator.type, "Literal");
    assertEq(stringLiteralWithEmbeddedTerminator.value, "\u2028");
    checkLoc(stringLiteralWithEmbeddedTerminator, [1, 329], [2, 1]);
  }
}
testGuaranteedSingleUnit();

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Testing completed");
