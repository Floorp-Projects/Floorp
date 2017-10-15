/**
 * Focus hyperlink invoker.
 *
 * @param aID             [in] hyperlink identifier
 * @param aSelectedAfter  [in] specifies if hyperlink is selected/focused after
 *                          the focus
 */
function focusLink(aID, aSelectedAfter) {
  this.node = getNode(aID);
  this.accessible = getAccessible(this.node);

  this.eventSeq = [];
  this.unexpectedEventSeq = [];

  var checker = new invokerChecker(EVENT_FOCUS, this.accessible);
  if (aSelectedAfter)
    this.eventSeq.push(checker);
  else
    this.unexpectedEventSeq.push(checker);

  this.invoke = function focusLink_invoke() {
    var expectedStates = (aSelectedAfter ? STATE_FOCUSABLE : 0);
    var unexpectedStates = (!aSelectedAfter ? STATE_FOCUSABLE : 0) | STATE_FOCUSED;
    testStates(aID, expectedStates, 0, unexpectedStates, 0);

    this.node.focus();
  };

  this.finalCheck = function focusLink_finalCheck() {
    var expectedStates = (aSelectedAfter ? STATE_FOCUSABLE | STATE_FOCUSED : 0);
    var unexpectedStates = (!aSelectedAfter ? STATE_FOCUSABLE | STATE_FOCUSED : 0);
    testStates(aID, expectedStates, 0, unexpectedStates, 0);
  };

  this.getID = function focusLink_getID() {
    return "focus hyperlink " + prettyName(aID);
  };
}
