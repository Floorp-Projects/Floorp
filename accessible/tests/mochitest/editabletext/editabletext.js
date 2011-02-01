/**
 * Perform all editable text tests.
 */
function editableTextTestRun()
{
  this.add = function add(aTest)
  {
    this.seq.push(aTest);
  }

  this.run = function run()
  {
    this.iterate();
  }

  this.index = 0;
  this.seq = new Array();

  this.iterate = function iterate()
  {
    if (this.index < this.seq.length) {
      this.seq[this.index++].startTest(this);
      return;
    }

    this.seq = null;
    SimpleTest.finish();
  }
}

/**
 * Used to test nsIEditableTextAccessible methods.
 */
function editableTextTest(aID)
{
  /**
   * setTextContents test.
   */
  this.setTextContents = function setTextContents(aStr, aResValue)
  {
    var testID = "setTextContents '" + aStr + "' for " + prettyName(aID);

    function setTextContentsInvoke()
    {
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.setTextContents(aStr);
    }

    this.scheduleTest(aID, null, [0, aStr.length, aStr],
                      setTextContentsInvoke, getValueChecker(aID, aResValue),
                      testID);
  }

  /**
   * insertText test.
   */
  this.insertText = function insertText(aStr, aPos, aResStr, aResPos)
  {
    var testID = "insertText '" + aStr + "' at " + aPos + " for " +
      prettyName(aID);

    function insertTextInvoke()
    {
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.insertText(aStr, aPos);
    }

    var resPos = (aResPos != undefined) ? aResPos : aPos;
    this.scheduleTest(aID, null, [resPos, resPos + aStr.length, aStr],
                      insertTextInvoke, getValueChecker(aID, aResStr), testID);
  }

  /**
   * copyText test.
   */
  this.copyText = function copyText(aStartPos, aEndPos, aClipboardStr)
  {
    var testID = "copyText from " + aStartPos + " to " + aEndPos + " for " +
      prettyName(aID);

    function copyTextInvoke()
    {
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.copyText(aStartPos, aEndPos);
    }

    this.scheduleTest(aID, null, null, copyTextInvoke,
                      getClipboardChecker(aID, aClipboardStr), testID);
  }

  /**
   * copyText and pasteText test.
   */
  this.copyNPasteText = function copyNPasteText(aStartPos, aEndPos,
                                                aPos, aResStr)
  {
    var testID = "copyText from " + aStartPos + " to " + aEndPos +
      "and pasteText at " + aPos + " for " + prettyName(aID);

    function copyNPasteTextInvoke()
    {
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.copyText(aStartPos, aEndPos);
      acc.pasteText(aPos);
    }

    this.scheduleTest(aID, null, [aStartPos, aEndPos, getTextFromClipboard],
                      copyNPasteInvoke, getValueChecker(aID, aResStr), testID);
  }

  /**
   * cutText test.
   */
  this.cutText = function cutText(aStartPos, aEndPos, aResStr,
                                  aResStartPos, aResEndPos)
  {
    var testID = "cutText from " + aStartPos + " to " + aEndPos + " for " +
      prettyName(aID);

    function cutTextInvoke()
    {
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.cutText(aStartPos, aEndPos);
    }

    var resStartPos = (aResStartPos != undefined) ? aResStartPos : aStartPos;
    var resEndPos = (aResEndPos != undefined) ? aResEndPos : aEndPos;
    this.scheduleTest(aID, [resStartPos, resEndPos, getTextFromClipboard], null,
                      cutTextInvoke, getValueChecker(aID, aResStr), testID);
  }

  /**
   * cutText and pasteText test.
   */
  this.cutNPasteText = function copyNPasteText(aStartPos, aEndPos,
                                               aPos, aResStr)
  {
    var testID = "cutText from " + aStartPos + " to " + aEndPos +
      " and pasteText at " + aPos + " for " + prettyName(aID);

    function cutNPasteTextInvoke()
    {
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.cutText(aStartPos, aEndPos);
      acc.pasteText(aPos);
    }

    this.scheduleTest(aID, [aStartPos, aEndPos, getTextFromClipboard],
                      [aPos, -1, getTextFromClipboard],
                      cutNPasteTextInvoke, getValueChecker(aID, aResStr),
                      testID);
  }

  /**
   * pasteText test.
   */
  this.pasteText = function pasteText(aPos, aResStr)
  {
    var testID = "pasteText at " + aPos + " for " + prettyName(aID);

    function pasteTextInvoke()
    {
      var acc = getAccessible(aID, nsIAccessibleEditableText);
      acc.pasteText(aPos);
    }

    this.scheduleTest(aID, null, [aPos, -1, getTextFromClipboard],
                      pasteTextInvoke, getValueChecker(aID, aResStr), testID);
  }

  /**
   * deleteText test.
   */
  this.deleteText = function deleteText(aStartPos, aEndPos, aResStr)
  {
    function getRemovedText() { return invoker.removedText; }

    var invoker = {
      eventSeq: [
        new textChangeChecker(aID, aStartPos, aEndPos, getRemovedText, false)
      ],

      invoke: function invoke()
      {
        var acc = getAccessible(aID,
                               [nsIAccessibleText, nsIAccessibleEditableText]);

        this.removedText = acc.getText(aStartPos, aEndPos);

        acc.deleteText(aStartPos, aEndPos);
      },

      finalCheck: function finalCheck()
      {
        getValueChecker(aID, aResStr).check();
      },

      getID: function getID()
      {
        return "deleteText from " + aStartPos + " to " + aEndPos + " for " +
          prettyName(aID);
      }
    };
    this.mEventQueue.push(invoker);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Implementation details.

  /**
   * Common checkers.
   */
  function getValueChecker(aID, aValue)
  {
    var checker = {
      check: function valueChecker_check()
      {
        var value = "";
        var elm = getNode(aID);
        if (elm instanceof Components.interfaces.nsIDOMNSEditableElement)
          value = elm.value;
        else if (elm instanceof Components.interfaces.nsIDOMHTMLDocument)
          value = elm.body.textContent;
        else
          value = elm.textContent;

        is(value, aValue, "Wrong value " + aValue);
      }
    };
    return checker;
  }

  function getClipboardChecker(aID, aText)
  {
    var checker = {
      check: function clipboardChecker_check()
      {
        is(getTextFromClipboard(), aText, "Wrong text in clipboard.");
      }
    };
    return checker;
  }

  function getValueNClipboardChecker(aID, aValue, aText)
  {
    var valueChecker = getValueChecker(aID, aValue);
    var clipboardChecker = getClipboardChecker(aID, aText);

    var checker = {
      check: function()
      {
        valueChecker.check();
        clipboardChecker.check();
      }
    };
    return checker;
  }

  /**
   * Create an invoker for the test and push it into event queue.
   */
  this.scheduleTest = function scheduleTest(aID, aRemoveTriple, aInsertTriple,
                                            aInvokeFunc, aChecker, aInvokerID)
  {
    var invoker = {
      eventSeq: [],

      invoke: aInvokeFunc,
      finalCheck: function finalCheck() { aChecker.check(); },
      getID: function getID() { return aInvokerID; }
    };

    if (aRemoveTriple) {
      var checker = new textChangeChecker(aID, aRemoveTriple[0],
                                          aRemoveTriple[1], aRemoveTriple[2],
                                          false);
      invoker.eventSeq.push(checker);
    }

    if (aInsertTriple) {
      var checker = new textChangeChecker(aID, aInsertTriple[0],
                                          aInsertTriple[1], aInsertTriple[2],
                                          true);
      invoker.eventSeq.push(checker);
    }

    this.mEventQueue.push(invoker);
  }

  /**
   * Run the tests.
   */
  this.startTest = function startTest(aTestRun)
  {
    var testRunObj = aTestRun;
    var thisObj = this;
    this.mEventQueue.onFinish = function finishCallback()
    {
      // Notify textRun object that all tests were finished.
      testRunObj.iterate();

      // Help GC to avoid leaks (refer to aTestRun from local variable, drop
      // onFinish function).
      thisObj.mEventQueue.onFinish = null;

      return DO_NOT_FINISH_TEST;
    }

    this.mEventQueue.invoke();
  }

  this.mEventQueue = new eventQueue();
}

