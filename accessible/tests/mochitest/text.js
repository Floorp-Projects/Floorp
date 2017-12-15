// //////////////////////////////////////////////////////////////////////////////
// Public

const BOUNDARY_CHAR = nsIAccessibleText.BOUNDARY_CHAR;
const BOUNDARY_WORD_START = nsIAccessibleText.BOUNDARY_WORD_START;
const BOUNDARY_WORD_END = nsIAccessibleText.BOUNDARY_WORD_END;
const BOUNDARY_LINE_START = nsIAccessibleText.BOUNDARY_LINE_START;
const BOUNDARY_LINE_END = nsIAccessibleText.BOUNDARY_LINE_END;

const kTextEndOffset = nsIAccessibleText.TEXT_OFFSET_END_OF_TEXT;
const kCaretOffset = nsIAccessibleText.TEXT_OFFSET_CARET;

const EndPoint_Start = nsIAccessibleTextRange.EndPoint_Start;
const EndPoint_End = nsIAccessibleTextRange.EndPoint_End;

const kTodo = 1; // a test is expected to fail
const kOk = 2; // a test doesn't fail

/**
 * Test characterCount for the given array of accessibles.
 *
 * @param aCount    [in] the expected character count
 * @param aIDs      [in] array of accessible identifiers to test
 * @param aTodoFlag [in, optional] either kOk or kTodo
 */
function testCharacterCount(aIDs, aCount, aTodoFlag) {
  var ids = (aIDs instanceof Array) ? aIDs : [ aIDs ];
  var isFunc = (aTodoFlag == kTodo) ? todo_is : is;
  for (var i = 0; i < ids.length; i++) {
    var textacc = getAccessible(ids[i], [nsIAccessibleText]);
    isFunc(textacc.characterCount, aCount,
           "Wrong character count for " + prettyName(ids[i]));
  }
}

/**
 * Test text between two given offsets.
 *
 * @param aIDs          [in] an array of accessible IDs to test
 * @param aStartOffset  [in] the start offset within the text to test
 * @param aEndOffset    [in] the end offset up to which the text is tested
 * @param aText         [in] the expected result from the test
 * @param aTodoFlag     [in, optional] either kOk or kTodo
 */
function testText(aIDs, aStartOffset, aEndOffset, aText, aTodoFlag) {
  var ids = (aIDs instanceof Array) ? aIDs : [ aIDs ];
  var isFunc = (aTodoFlag == kTodo) ? todo_is : is;
  for (var i = 0; i < ids.length; i++) {
    var acc = getAccessible(ids[i], nsIAccessibleText);
    try {
      isFunc(acc.getText(aStartOffset, aEndOffset), aText,
             "getText: wrong text between start and end offsets '" +
             aStartOffset + "', '" + aEndOffset + " for '" +
             prettyName(ids[i]) + "'");
    } catch (e) {
      ok(false,
        "getText fails between start and end offsets '" + aStartOffset +
        "', '" + aEndOffset + " for '" + prettyName(ids[i]) + "'");
    }
  }
}

/**
 * Test password text between two given offsets
 *
 * @param aIDs          [in] an array of accessible IDs to test
 * @param aStartOffset  [in] the start offset within the text to test
 * @param aEndOffset    [in] the end offset up to which the text is tested
 * @param aText         [in] the expected result from the test
 *
 * @note  All this function does is test that getText doe snot expose the
 *        password text itself, but something else.
 */
function testPasswordText(aIDs, aStartOffset, aEndOffset, aText) {
  for (var i = 0; i < aIDs.length; i++) {
    var acc = getAccessible(aIDs[i], nsIAccessibleText);
    try {
      isnot(acc.getText(aStartOffset, aEndOffset), aText,
         "getText: plain text between start and end offsets '" + aStartOffset +
         "', '" + aEndOffset + " for '" + prettyName(aIDs[i]) + "'");
    } catch (e) {
      ok(false,
         "getText fails between start and end offsets '" + aStartOffset +
         "', '" + aEndOffset + " for '" + prettyName(aIDs[i]) + "'");
    }
  }
}

/**
 * Test getTextAtOffset for BOUNDARY_CHAR over different elements.
 *
 * @param aIDs          [in] the accessible identifier or array of accessible
 *                        identifiers
 * @param aOffset       [in] the offset to get a character at it
 * @param aChar         [in] the expected character
 * @param aStartOffset  [in] expected start offset of the character
 * @param aEndOffset    [in] expected end offset of the character
 */
function testCharAtOffset(aIDs, aOffset, aChar, aStartOffset, aEndOffset) {
  var IDs = (aIDs instanceof Array) ? aIDs : [ aIDs ];
  for (var i = 0; i < IDs.length; i++) {
    var acc = getAccessible(IDs[i], nsIAccessibleText);
    testTextHelper(IDs[i], aOffset, BOUNDARY_CHAR,
                   aChar, aStartOffset, aEndOffset,
                   kOk, kOk, kOk,
                   acc.getTextAtOffset, "getTextAtOffset ");
  }
}

/**
 * Test getTextAtOffset function over different elements.
 *
 * @param aIDs            [in] ID or array of IDs
 * @param aBoundaryType   [in] boundary type for text to be retrieved
 * @param aTestList       [in] array of sets:
 *                              offset1 and offset2 defining the offset range
 *                              the text in the range
 *                              start offset of the text in the range
 *                              end offset of the text in the range
 *
 * or
 *
 * @param aOffset         [in] the offset to get the text at
 * @param aBoundaryType   [in] Boundary type for text to be retrieved
 * @param aText           [in] expected return text for getTextAtOffset
 * @param aStartOffset    [in] expected return start offset for getTextAtOffset
 * @param aEndOffset      [in] expected return end offset for getTextAtOffset
 * @param ...             [in] list of ids or list of tuples made of:
 *                              element identifier
 *                              kTodo or kOk for returned text
 *                              kTodo or kOk for returned start offset
 *                              kTodo or kOk for returned offset result
 */
function testTextAtOffset() {
  testTextSuperHelper("getTextAtOffset", arguments);
}

/**
 * Test getTextAfterOffset for BOUNDARY_CHAR over different elements.
 *
 * @param aIDs          [in] the accessible identifier or array of accessible
 *                        identifiers
 * @param aOffset       [in] the offset to get a character after it
 * @param aChar         [in] the expected character
 * @param aStartOffset  [in] expected start offset of the character
 * @param aEndOffset    [in] expected end offset of the character
 */
function testCharAfterOffset(aIDs, aOffset, aChar, aStartOffset, aEndOffset) {
  var IDs = (aIDs instanceof Array) ? aIDs : [ aIDs ];
  for (var i = 0; i < IDs.length; i++) {
    var acc = getAccessible(IDs[i], nsIAccessibleText);
    testTextHelper(IDs[i], aOffset, BOUNDARY_CHAR,
                   aChar, aStartOffset, aEndOffset,
                   kOk, kOk, kOk,
                   acc.getTextAfterOffset, "getTextAfterOffset ");
  }
}

/**
 * Test getTextAfterOffset function over different elements
 *
 * @param aIDs            [in] ID or array of IDs
 * @param aBoundaryType   [in] boundary type for text to be retrieved
 * @param aTestList       [in] array of sets:
 *                              offset1 and offset2 defining the offset range
 *                              the text in the range
 *                              start offset of the text in the range
 *                              end offset of the text in the range
 *
 * or
 *
 * @param aOffset         [in] the offset to get the text after
 * @param aBoundaryType   [in] Boundary type for text to be retrieved
 * @param aText           [in] expected return text for getTextAfterOffset
 * @param aStartOffset    [in] expected return start offset for getTextAfterOffset
 * @param aEndOffset      [in] expected return end offset for getTextAfterOffset
 * @param ...             [in] list of ids or list of tuples made of:
 *                              element identifier
 *                              kTodo or kOk for returned text
 *                              kTodo or kOk for returned start offset
 *                              kTodo or kOk for returned offset result
 */
function testTextAfterOffset(aOffset, aBoundaryType,
                             aText, aStartOffset, aEndOffset) {
  testTextSuperHelper("getTextAfterOffset", arguments);
}

/**
 * Test getTextBeforeOffset for BOUNDARY_CHAR over different elements.
 *
 * @param aIDs          [in] the accessible identifier or array of accessible
 *                        identifiers
 * @param aOffset       [in] the offset to get a character before it
 * @param aChar         [in] the expected character
 * @param aStartOffset  [in] expected start offset of the character
 * @param aEndOffset    [in] expected end offset of the character
 */
function testCharBeforeOffset(aIDs, aOffset, aChar, aStartOffset, aEndOffset) {
  var IDs = (aIDs instanceof Array) ? aIDs : [ aIDs ];
  for (var i = 0; i < IDs.length; i++) {
    var acc = getAccessible(IDs[i], nsIAccessibleText);
    testTextHelper(IDs[i], aOffset, BOUNDARY_CHAR,
                   aChar, aStartOffset, aEndOffset,
                   kOk, kOk, kOk,
                   acc.getTextBeforeOffset, "getTextBeforeOffset ");
  }
}

/**
 * Test getTextBeforeOffset function over different elements
 *
 * @param aIDs            [in] ID or array of IDs
 * @param aBoundaryType   [in] boundary type for text to be retrieved
 * @param aTestList       [in] array of sets:
 *                              offset1 and offset2 defining the offset range
 *                              the text in the range
 *                              start offset of the text in the range
 *                              end offset of the text in the range
 *
 * or
 *
 * @param aOffset         [in] the offset to get the text before
 * @param aBoundaryType   [in] Boundary type for text to be retrieved
 * @param aText           [in] expected return text for getTextBeforeOffset
 * @param aStartOffset    [in] expected return start offset for getTextBeforeOffset
 * @param aEndOffset      [in] expected return end offset for getTextBeforeOffset
 * @param ...             [in] list of ids or list of tuples made of:
 *                              element identifier
 *                              kTodo or kOk for returned text
 *                              kTodo or kOk for returned start offset
 *                              kTodo or kOk for returned offset result
 */
function testTextBeforeOffset(aOffset, aBoundaryType,
                              aText, aStartOffset, aEndOffset) {
  testTextSuperHelper("getTextBeforeOffset", arguments);
}

/**
 * Test word count for an element.
 *
 * @param aElement   [in] element identifier
 * @param aCount     [in] Expected word count
 * @param aToDoFlag  [in] kTodo or kOk for returned text
 */
function testWordCount(aElement, aCount, aToDoFlag) {
  var isFunc = (aToDoFlag == kTodo) ? todo_is : is;
  var acc = getAccessible(aElement, nsIAccessibleText);
  var startOffsetObj = {}, endOffsetObj = {};
  var length = acc.characterCount;
  var offset = 0;
  var wordCount = 0;
  while (true) {
    acc.getTextAtOffset(offset, BOUNDARY_WORD_START,
                        startOffsetObj, endOffsetObj);
    if (offset >= length)
      break;

    wordCount++;
    offset = endOffsetObj.value;
  }
  isFunc(wordCount, aCount,
        "wrong words count for '" + acc.getText(0, -1) + "': " + wordCount +
        " in " + prettyName(aElement));
}

/**
 * Test word at a position for an element.
 *
 * @param aElement    [in] element identifier
 * @param aWordIndex  [in] index of the word to test
 * @param aText       [in] expected text for that word
 * @param aToDoFlag   [in] kTodo or kOk for returned text
 */
function testWordAt(aElement, aWordIndex, aText, aToDoFlag) {
  var isFunc = (aToDoFlag == kTodo) ? todo_is : is;
  var acc = getAccessible(aElement, nsIAccessibleText);

  var textLength = acc.characterCount;
  var wordIdx = aWordIndex;
  var startOffsetObj = { value: 0 }, endOffsetObj = { value: 0 };
  for (offset = 0; offset < textLength; offset = endOffsetObj.value) {
    acc.getTextAtOffset(offset, BOUNDARY_WORD_START,
                        startOffsetObj, endOffsetObj);

    wordIdx--;
    if (wordIdx < 0)
      break;
  }

  if (wordIdx >= 0) {
    ok(false,
       "the given word index '" + aWordIndex + "' exceeds words amount in " +
       prettyName(aElement));

    return;
  }

  var startWordOffset = startOffsetObj.value;
  var endWordOffset = endOffsetObj.value;

  // Calculate the end word offset.
  acc.getTextAtOffset(endOffsetObj.value, BOUNDARY_WORD_END,
                      startOffsetObj, endOffsetObj);
  if (startOffsetObj.value != textLength)
    endWordOffset = startOffsetObj.value;

  if (endWordOffset <= startWordOffset) {
    todo(false,
         "wrong start and end offset for word at index '" + aWordIndex + "': " +
         " of text '" + acc.getText(0, -1) + "' in " + prettyName(aElement));

    return;
  }

  text = acc.getText(startWordOffset, endWordOffset);
  isFunc(text, aText, "wrong text for word at index '" + aWordIndex + "': " +
         " of text '" + acc.getText(0, -1) + "' in " + prettyName(aElement));
}

/**
 * Test words in a element.
 *
 * @param aElement   [in]           element identifier
 * @param aWords     [in]           array of expected words
 * @param aToDoFlag  [in, optional] kTodo or kOk for returned text
 */
function testWords(aElement, aWords, aToDoFlag) {
  if (aToDoFlag == null)
    aToDoFlag = kOk;

  testWordCount(aElement, aWords.length, aToDoFlag);

  for (var i = 0; i < aWords.length; i++) {
    testWordAt(aElement, i, aWords[i], aToDoFlag);
  }
}

/**
 * Remove all selections.
 *
 * @param aID  [in] Id, DOM node, or acc obj
 */
function cleanTextSelections(aID) {
  var acc = getAccessible(aID, [nsIAccessibleText]);

  while (acc.selectionCount > 0)
    acc.removeSelection(0);
}

/**
 * Test addSelection method.
 *
 * @param aID               [in] Id, DOM node, or acc obj
 * @param aStartOffset      [in] start offset for the new selection
 * @param aEndOffset        [in] end offset for the new selection
 * @param aSelectionsCount  [in] expected number of selections after addSelection
 */
function testTextAddSelection(aID, aStartOffset, aEndOffset, aSelectionsCount) {
  var acc = getAccessible(aID, [nsIAccessibleText]);
  var text = acc.getText(0, -1);

  acc.addSelection(aStartOffset, aEndOffset);

  ok(acc.selectionCount, aSelectionsCount,
     text + ": failed to add selection from offset '" + aStartOffset +
     "' to offset '" + aEndOffset + "': selectionCount after");
}

/**
 * Test removeSelection method.
 *
 * @param aID               [in] Id, DOM node, or acc obj
 * @param aSelectionIndex   [in] index of the selection to be removed
 * @param aSelectionsCount  [in] expected number of selections after
 *                               removeSelection
 */
function testTextRemoveSelection(aID, aSelectionIndex, aSelectionsCount) {
  var acc = getAccessible(aID, [nsIAccessibleText]);
  var text = acc.getText(0, -1);

  acc.removeSelection(aSelectionIndex);

  ok(acc.selectionCount, aSelectionsCount,
     text + ": failed to remove selection at index '" +
     aSelectionIndex + "': selectionCount after");
}

/**
 * Test setSelectionBounds method.
 *
 * @param aID               [in] Id, DOM node, or acc obj
 * @param aStartOffset      [in] new start offset for the selection
 * @param aEndOffset        [in] new end offset for the selection
 * @param aSelectionIndex   [in] index of the selection to set
 * @param aSelectionsCount  [in] expected number of selections after
 *                               setSelectionBounds
 */
function testTextSetSelection(aID, aStartOffset, aEndOffset,
                              aSelectionIndex, aSelectionsCount) {
  var acc = getAccessible(aID, [nsIAccessibleText]);
  var text = acc.getText(0, -1);

  acc.setSelectionBounds(aSelectionIndex, aStartOffset, aEndOffset);

  is(acc.selectionCount, aSelectionsCount,
     text + ": failed to set selection at index '" +
     aSelectionIndex + "': selectionCount after");
}

/**
 * Test selectionCount method.
 *
 * @param aID        [in] Id, DOM node, or acc obj
 * @param aCount     [in] expected selection count
 */
function testTextSelectionCount(aID, aCount) {
  var acc = getAccessible(aID, [nsIAccessibleText]);
  var text = acc.getText(0, -1);

  is(acc.selectionCount, aCount, text + ": wrong selectionCount: ");
}

/**
 * Test getSelectionBounds method.
 *
 * @param aID              [in] Id, DOM node, or acc obj
 * @param aStartOffset     [in] expected start offset for the selection
 * @param aEndOffset       [in] expected end offset for the selection
 * @param aSelectionIndex  [in] index of the selection to get
 */
function testTextGetSelection(aID, aStartOffset, aEndOffset, aSelectionIndex) {
  var acc = getAccessible(aID, [nsIAccessibleText]);
  var text = acc.getText(0, -1);

  var startObj = {}, endObj = {};
  acc.getSelectionBounds(aSelectionIndex, startObj, endObj);

  is(startObj.value, aStartOffset, text + ": wrong start offset for index '" +
     aSelectionIndex + "'");
  is(endObj.value, aEndOffset, text + ": wrong end offset for index '" +
     aSelectionIndex + "'");
}

function testTextRange(aRange, aRangeDescr, aStartContainer, aStartOffset,
                       aEndContainer, aEndOffset, aText,
                       aCommonContainer, aChildren) {
  isObject(aRange.startContainer, getAccessible(aStartContainer),
           "Wrong start container of " + aRangeDescr);
  is(aRange.startOffset, aStartOffset,
     "Wrong start offset of " + aRangeDescr);
  isObject(aRange.endContainer, getAccessible(aEndContainer),
           "Wrong end container of " + aRangeDescr);
  is(aRange.endOffset, aEndOffset,
     "Wrong end offset of " + aRangeDescr);

  if (aText === undefined) {
    return;
  }

  is(aRange.text, aText, "Wrong text of " + aRangeDescr);

  var children = aRange.embeddedChildren;
  is(children ? children.length : 0, aChildren ? aChildren.length : 0,
     "Wrong embedded children count of " + aRangeDescr);

  isObject(aRange.container, getAccessible(aCommonContainer),
           "Wrong container of " + aRangeDescr);

  if (aChildren) {
    for (var i = 0; i < aChildren.length; i++) {
      var expectedChild = getAccessible(aChildren[i]);
      var actualChild = children.queryElementAt(i, nsIAccessible);
      isObject(actualChild, expectedChild,
               "Wrong child at index '" + i + "' of " + aRangeDescr);
    }
  }
}

// //////////////////////////////////////////////////////////////////////////////
// Private

function testTextSuperHelper(aFuncName, aArgs) {
  // List of tests.
  if (aArgs[2] instanceof Array) {
    let ids = (aArgs[0] instanceof Array) ? aArgs[0] : [ aArgs[0] ];
    let boundaryType = aArgs[1];
    let list = aArgs[2];
    for (let i = 0; i < list.length; i++) {
      let offset1 = list[i][0], offset2 = list[i][1];
      let text = list[i][2], startOffset = list[i][3], endOffset = list[i][4];
      let failureList = list[i][5];
      for (let offset = offset1; offset <= offset2; offset++) {
        for (let idIdx = 0; idIdx < ids.length; idIdx++) {
          let id = ids[idIdx];

          let flagOk1 = kOk, flagOk2 = kOk, flagOk3 = kOk;
          if (failureList) {
            for (let fIdx = 0; fIdx < failureList.length; fIdx++) {
              if (offset == failureList[fIdx][0] && id == failureList[fIdx][1]) {
                flagOk1 = failureList[fIdx][2];
                flagOk2 = failureList[fIdx][3];
                flagOk3 = failureList[fIdx][4];
                break;
              }
            }
          }

          let acc = getAccessible(id, nsIAccessibleText);
          testTextHelper(id, offset, boundaryType,
                         text, startOffset, endOffset,
                         flagOk1, flagOk2, flagOk3,
                         acc[aFuncName], aFuncName + " ");
        }
      }
    }
    return;
  }

  // Test at single offset. List of IDs.
  var offset = aArgs[0];
  var boundaryType = aArgs[1];
  var text = aArgs[2];
  var startOffset = aArgs[3];
  var endOffset = aArgs[4];
  if (aArgs[5] instanceof Array) {
    let ids = aArgs[5];
    for (let i = 0; i < ids.length; i++) {
      let acc = getAccessible(ids[i], nsIAccessibleText);
      testTextHelper(ids[i], offset, boundaryType,
                     text, startOffset, endOffset,
                     kOk, kOk, kOk,
                     acc[aFuncName], aFuncName + " ");
    }

    return;
  }

  // Each ID is tested separately.
  for (let i = 5; i < aArgs.length; i = i + 4) {
    let ID = aArgs[i];
    let acc = getAccessible(ID, nsIAccessibleText);
    let toDoFlag1 = aArgs[i + 1];
    let toDoFlag2 = aArgs[i + 2];
    let toDoFlag3 = aArgs[i + 3];

    testTextHelper(ID, offset, boundaryType,
                   text, startOffset, endOffset,
                   toDoFlag1, toDoFlag2, toDoFlag3,
                   acc[aFuncName], aFuncName + " ");
  }
}

function testTextHelper(aID, aOffset, aBoundaryType,
                        aText, aStartOffset, aEndOffset,
                        aToDoFlag1, aToDoFlag2, aToDoFlag3,
                        aTextFunc, aTextFuncName) {
  var exceptionFlag = aToDoFlag1 == undefined ||
                      aToDoFlag2 == undefined ||
                      aToDoFlag3 == undefined;

  var startMsg = aTextFuncName + "(" + boundaryToString(aBoundaryType) + "): ";
  var endMsg = ", id: " + prettyName(aID) + ";";

  try {
    var startOffsetObj = {}, endOffsetObj = {};
    var text = aTextFunc(aOffset, aBoundaryType,
                         startOffsetObj, endOffsetObj);

    if (exceptionFlag) {
      ok(false, startMsg + "no expected failure at offset " + aOffset + endMsg);
      return;
    }

    var isFunc1 = (aToDoFlag1 == kTodo) ? todo : ok;
    var isFunc2 = (aToDoFlag2 == kTodo) ? todo : ok;
    var isFunc3 = (aToDoFlag3 == kTodo) ? todo : ok;

    isFunc1(text == aText,
            startMsg + "wrong text " +
            "(got '" + text + "', expected: '" + aText + "')" +
            ", offset: " + aOffset + endMsg);
    isFunc2(startOffsetObj.value == aStartOffset,
            startMsg + "wrong start offset" +
            "(got '" + startOffsetObj.value + "', expected: '" + aStartOffset + "')" +
            ", offset: " + aOffset + endMsg);
    isFunc3(endOffsetObj.value == aEndOffset,
            startMsg + "wrong end offset" +
            "(got '" + endOffsetObj.value + "', expected: '" + aEndOffset + "')" +
            ", offset: " + aOffset + endMsg);

  } catch (e) {
    var okFunc = exceptionFlag ? todo : ok;
    okFunc(false, startMsg + "failed at offset " + aOffset + endMsg +
           ", exception: " + e);
  }
}

function boundaryToString(aBoundaryType) {
  switch (aBoundaryType) {
    case BOUNDARY_CHAR:
      return "char";
    case BOUNDARY_WORD_START:
      return "word start";
    case BOUNDARY_WORD_END:
      return "word end";
    case BOUNDARY_LINE_START:
      return "line start";
    case BOUNDARY_LINE_END:
      return "line end";
  }
}
