/* import-globals-from ../common.js */
/* import-globals-from ../events.js */

/**
 * Perform all editable text tests.
 */
function editableTextTestRun() {
  this.add = function add(aTest) {
    this.seq.push(aTest);
  };

  this.run = function run() {
    this.iterate();
  };

  this.index = 0;
  this.seq = [];

  this.iterate = function iterate() {
    if (this.index < this.seq.length) {
      this.seq[this.index++].startTest(this);
      return;
    }

    this.seq = null;
    SimpleTest.finish();
  };
}

/**
 * Used to test nsIEditableTextAccessible methods.
 */
function editableTextTest(aID) {
  /**
   * Schedule a test, the given function with its arguments will be executed
   * when preceding test is complete.
   */
  this.scheduleTest = function scheduleTest(aFunc, ...aFuncArgs) {
    // A data container acts like a dummy invoker, it's never invoked but
    // it's used to generate real invoker when previous invoker was handled.
    var dataContainer = {
      func: aFunc,
      funcArgs: aFuncArgs,
    };
    this.mEventQueue.push(dataContainer);

    if (!this.mEventQueueReady) {
      this.unwrapNextTest();
      this.mEventQueueReady = true;
    }
  };

  /**
   * setTextContents test.
   */
  this.setTextContents = function setTextContents(aValue, aSkipStartOffset) {
    var testID = "setTextContents '" + aValue + "' for " + prettyName(aID);

    function setTextContentsInvoke() {
      dump(`\nsetTextContents '${aValue}'\n`);
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.setTextContents(aValue);
    }

    aSkipStartOffset = aSkipStartOffset || 0;
    var insertTripple = aValue
      ? [aSkipStartOffset, aSkipStartOffset + aValue.length, aValue]
      : null;
    var oldValue = getValue();
    var removeTripple = oldValue
      ? [aSkipStartOffset, aSkipStartOffset + oldValue.length, oldValue]
      : null;

    this.generateTest(
      removeTripple,
      insertTripple,
      setTextContentsInvoke,
      getValueChecker(aValue),
      testID
    );
  };

  /**
   * insertText test.
   */
  this.insertText = function insertText(aStr, aPos, aResStr, aResPos) {
    var testID =
      "insertText '" + aStr + "' at " + aPos + " for " + prettyName(aID);

    function insertTextInvoke() {
      dump(`\ninsertText '${aStr}' at ${aPos} pos\n`);
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.insertText(aStr, aPos);
    }

    var resPos = aResPos != undefined ? aResPos : aPos;
    this.generateTest(
      null,
      [resPos, resPos + aStr.length, aStr],
      insertTextInvoke,
      getValueChecker(aResStr),
      testID
    );
  };

  /**
   * copyText test.
   */
  this.copyText = function copyText(aStartPos, aEndPos, aClipboardStr) {
    var testID =
      "copyText from " +
      aStartPos +
      " to " +
      aEndPos +
      " for " +
      prettyName(aID);

    function copyTextInvoke() {
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.copyText(aStartPos, aEndPos);
    }

    this.generateTest(
      null,
      null,
      copyTextInvoke,
      getClipboardChecker(aClipboardStr),
      testID
    );
  };

  /**
   * copyText and pasteText test.
   */
  this.copyNPasteText = function copyNPasteText(
    aStartPos,
    aEndPos,
    aPos,
    aResStr
  ) {
    var testID =
      "copyText from " +
      aStartPos +
      " to " +
      aEndPos +
      "and pasteText at " +
      aPos +
      " for " +
      prettyName(aID);

    function copyNPasteTextInvoke() {
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.copyText(aStartPos, aEndPos);
      acc.pasteText(aPos);
    }

    this.generateTest(
      null,
      [aStartPos, aEndPos, getTextFromClipboard],
      copyNPasteTextInvoke,
      getValueChecker(aResStr),
      testID
    );
  };

  /**
   * cutText test.
   */
  this.cutText = function cutText(
    aStartPos,
    aEndPos,
    aResStr,
    aResStartPos,
    aResEndPos
  ) {
    var testID =
      "cutText from " +
      aStartPos +
      " to " +
      aEndPos +
      " for " +
      prettyName(aID);

    function cutTextInvoke() {
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.cutText(aStartPos, aEndPos);
    }

    var resStartPos = aResStartPos != undefined ? aResStartPos : aStartPos;
    var resEndPos = aResEndPos != undefined ? aResEndPos : aEndPos;
    this.generateTest(
      [resStartPos, resEndPos, getTextFromClipboard],
      null,
      cutTextInvoke,
      getValueChecker(aResStr),
      testID
    );
  };

  /**
   * cutText and pasteText test.
   */
  this.cutNPasteText = function copyNPasteText(
    aStartPos,
    aEndPos,
    aPos,
    aResStr
  ) {
    var testID =
      "cutText from " +
      aStartPos +
      " to " +
      aEndPos +
      " and pasteText at " +
      aPos +
      " for " +
      prettyName(aID);

    function cutNPasteTextInvoke() {
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.cutText(aStartPos, aEndPos);
      acc.pasteText(aPos);
    }

    this.generateTest(
      [aStartPos, aEndPos, getTextFromClipboard],
      [aPos, -1, getTextFromClipboard],
      cutNPasteTextInvoke,
      getValueChecker(aResStr),
      testID
    );
  };

  /**
   * pasteText test.
   */
  this.pasteText = function pasteText(aPos, aResStr) {
    var testID = "pasteText at " + aPos + " for " + prettyName(aID);

    function pasteTextInvoke() {
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.pasteText(aPos);
    }

    this.generateTest(
      null,
      [aPos, -1, getTextFromClipboard],
      pasteTextInvoke,
      getValueChecker(aResStr),
      testID
    );
  };

  /**
   * deleteText test.
   */
  this.deleteText = function deleteText(aStartPos, aEndPos, aResStr) {
    var testID =
      "deleteText from " +
      aStartPos +
      " to " +
      aEndPos +
      " for " +
      prettyName(aID);

    var oldValue = getValue().substring(aStartPos, aEndPos);
    var removeTripple = oldValue ? [aStartPos, aEndPos, oldValue] : null;

    function deleteTextInvoke() {
      var acc = getAccessible(aID, [nsIAccessibleEditableText]);
      acc.deleteText(aStartPos, aEndPos);
    }

    this.generateTest(
      removeTripple,
      null,
      deleteTextInvoke,
      getValueChecker(aResStr),
      testID
    );
  };

  // ////////////////////////////////////////////////////////////////////////////
  // Implementation details.

  function getValue() {
    var elm = getNode(aID);
    var elmClass = ChromeUtils.getClassName(elm);
    if (elmClass === "HTMLTextAreaElement" || elmClass === "HTMLInputElement") {
      return elm.value;
    }

    if (elmClass === "HTMLDocument") {
      return elm.body.textContent;
    }

    return elm.textContent;
  }

  /**
   * Common checkers.
   */
  function getValueChecker(aValue) {
    var checker = {
      check: function valueChecker_check() {
        is(getValue(), aValue, "Wrong value " + aValue);
      },
    };
    return checker;
  }

  function getClipboardChecker(aText) {
    var checker = {
      check: function clipboardChecker_check() {
        is(getTextFromClipboard(), aText, "Wrong text in clipboard.");
      },
    };
    return checker;
  }

  /**
   * Process next scheduled test.
   */
  this.unwrapNextTest = function unwrapNextTest() {
    var data = this.mEventQueue.mInvokers[this.mEventQueue.mIndex + 1];
    if (data) {
      data.func.apply(this, data.funcArgs);
    }
  };

  /**
   * Used to generate an invoker object for the sheduled test.
   */
  this.generateTest = function generateTest(
    aRemoveTriple,
    aInsertTriple,
    aInvokeFunc,
    aChecker,
    aInvokerID
  ) {
    var et = this;
    var invoker = {
      eventSeq: [],

      invoke: aInvokeFunc,
      finalCheck: function finalCheck() {
        // dumpTree(aID, `'${aID}' tree:`);

        aChecker.check();
        et.unwrapNextTest(); // replace dummy invoker on real invoker object.
      },
      getID: function getID() {
        return aInvokerID;
      },
    };

    if (aRemoveTriple) {
      let checker = new textChangeChecker(
        aID,
        aRemoveTriple[0],
        aRemoveTriple[1],
        aRemoveTriple[2],
        false
      );
      invoker.eventSeq.push(checker);
    }

    if (aInsertTriple) {
      let checker = new textChangeChecker(
        aID,
        aInsertTriple[0],
        aInsertTriple[1],
        aInsertTriple[2],
        true
      );
      invoker.eventSeq.push(checker);
    }

    // Claim that we don't want to fail when no events are expected.
    if (!aRemoveTriple && !aInsertTriple) {
      invoker.noEventsOnAction = true;
    }

    this.mEventQueue.mInvokers[this.mEventQueue.mIndex + 1] = invoker;
  };

  /**
   * Run the tests.
   */
  this.startTest = function startTest(aTestRun) {
    var testRunObj = aTestRun;
    var thisObj = this;
    this.mEventQueue.onFinish = function finishCallback() {
      // Notify textRun object that all tests were finished.
      testRunObj.iterate();

      // Help GC to avoid leaks (refer to aTestRun from local variable, drop
      // onFinish function).
      thisObj.mEventQueue.onFinish = null;

      return DO_NOT_FINISH_TEST;
    };

    this.mEventQueue.invoke();
  };

  this.mEventQueue = new eventQueue();
  this.mEventQueueReady = false;
}
