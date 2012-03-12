Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

////////////////////////////////////////////////////////////////////////////////
// Constants

const PREFILTER_INVISIBLE = nsIAccessibleTraversalRule.PREFILTER_INVISIBLE;
const FILTER_MATCH = nsIAccessibleTraversalRule.FILTER_MATCH;
const FILTER_IGNORE = nsIAccessibleTraversalRule.FILTER_IGNORE;
const FILTER_IGNORE_SUBTREE = nsIAccessibleTraversalRule.FILTER_IGNORE_SUBTREE;

////////////////////////////////////////////////////////////////////////////////
// Traversal rules

/**
 * Rule object to traverse all focusable nodes and text nodes.
 */
var HeadersTraversalRule =
{
  getMatchRoles: function(aRules)
  {
    aRules.value = [ROLE_HEADING];
    return aRules.value.length;
  },

  preFilter: PREFILTER_INVISIBLE,

  match: function(aAccessible)
  {
    return FILTER_MATCH;
  },

  QueryInterface: XPCOMUtils.generateQI([nsIAccessibleTraversalRule])
}

/**
 * Traversal rule for all focusable nodes or leafs.
 */
var ObjectTraversalRule =
{
  getMatchRoles: function(aRules)
  {
    aRules.value = [];
    return 0;
  },

  preFilter: PREFILTER_INVISIBLE,

  match: function(aAccessible)
  {
    var rv = FILTER_IGNORE;
    var role = aAccessible.role;
    if (hasState(aAccessible, STATE_FOCUSABLE) &&
        (role != ROLE_DOCUMENT && role != ROLE_INTERNAL_FRAME))
      rv = FILTER_IGNORE_SUBTREE | FILTER_MATCH;
    else if (aAccessible.childCount == 0 &&
             role != ROLE_STATICTEXT && aAccessible.name.trim())
      rv = FILTER_MATCH;

    return rv;
  },

  QueryInterface: XPCOMUtils.generateQI([nsIAccessibleTraversalRule])
};

////////////////////////////////////////////////////////////////////////////////
// Virtual state invokers and checkers

/**
 * A checker for virtual cursor changed events.
 */
function virtualCursorChangedChecker(aDocAcc, aIdOrNameOrAcc, aTextOffsets)
{
  this.__proto__ = new invokerChecker(EVENT_VIRTUALCURSOR_CHANGED, aDocAcc);

  this.check = function virtualCursorChangedChecker_check(aEvent)
  {
    SimpleTest.info("virtualCursorChangedChecker_check");

    var event = null;
    try {
      event = aEvent.QueryInterface(nsIAccessibleVirtualCursorChangeEvent);
    } catch (e) {
      SimpleTest.ok(false, "Does not support correct interface: " + e);
    }

    var position = aDocAcc.virtualCursor.position;

    var idMatches = position.DOMNode.id == aIdOrNameOrAcc;
    var nameMatches = position.name == aIdOrNameOrAcc;
    var accMatches = position == aIdOrNameOrAcc;

    SimpleTest.ok(idMatches || nameMatches || accMatches, "id or name matches",
                  "expecting " + aIdOrNameOrAcc + ", got '" +
                  prettyName(position));

    if (aTextOffsets) {
      SimpleTest.is(aDocAcc.virtualCursor.startOffset, aTextOffsets[0],
                    "wrong start offset");
      SimpleTest.is(aDocAcc.virtualCursor.endOffset, aTextOffsets[1],
                    "wrong end offset");
    }

    var prevPosAndOffset = virtualCursorChangedChecker.
      getPreviousPosAndOffset(aDocAcc.virtualCursor);

    if (prevPosAndOffset) {
      SimpleTest.is(event.oldAccessible, prevPosAndOffset.position,
                    "previous position does not match");
      SimpleTest.is(event.oldStartOffset, prevPosAndOffset.startOffset,
                    "previous start offset does not match");
      SimpleTest.is(event.oldEndOffset, prevPosAndOffset.endOffset,
                    "previous end offset does not match");
    }
  };
}

virtualCursorChangedChecker.prevPosAndOffset = {};

virtualCursorChangedChecker.storePreviousPosAndOffset =
  function storePreviousPosAndOffset(aPivot)
{
  virtualCursorChangedChecker.prevPosAndOffset[aPivot] =
    {position: aPivot.position,
     startOffset: aPivot.startOffset,
     endOffset: aPivot.endOffset};
};

virtualCursorChangedChecker.getPreviousPosAndOffset =
  function getPreviousPosAndOffset(aPivot)
{
  return virtualCursorChangedChecker.prevPosAndOffset[aPivot];
};

/**
 * Set a text range in the pivot and wait for virtual cursor change event.
 *
 * @param aDocAcc document that manages the virtual cursor
 * @param aTextAccessible accessible to set to virtual cursor's position
 * @param aTextOffsets start and end offsets of text range to set in virtual
 *   cursor
 */
function setVirtualCursorRangeInvoker(aDocAcc, aTextAccessible, aTextOffsets)
{
  this.invoke = function virtualCursorChangedInvoker_invoke()
  {
    virtualCursorChangedChecker.
      storePreviousPosAndOffset(aDocAcc.virtualCursor);
    SimpleTest.info(prettyName(aTextAccessible) + " " + aTextOffsets);
    aDocAcc.virtualCursor.setTextRange(aTextAccessible,
                                       aTextOffsets[0],
                                       aTextOffsets[1]);
  };

  this.getID = function setVirtualCursorRangeInvoker_getID()
  {
    return "Set offset in " + prettyName(aTextAccessible) +
      " to (" + aTextOffsets[0] + ", " + aTextOffsets[1] + ")";
  }

  this.eventSeq = [
    new virtualCursorChangedChecker(aDocAcc, aTextAccessible, aTextOffsets)
  ];
}

/**
 * Move the pivot and wait for virtual cursor change event.
 *
 * @param aDocAcc document that manages the virtual cursor
 * @param aPivotMoveMethod method to test (ie. "moveNext", "moveFirst", etc.)
 * @param aRule traversal rule object
 * @param aIdOrNameOrAcc id, accessivle or accessible name to expect virtual
 *   cursor to land on after performing move method.
 */
function setVirtualCursorPosInvoker(aDocAcc, aPivotMoveMethod, aRule,
                                    aIdOrNameOrAcc)
{
  this.invoke = function virtualCursorChangedInvoker_invoke()
  {
    virtualCursorChangedChecker.
      storePreviousPosAndOffset(aDocAcc.virtualCursor);
    var moved = aDocAcc.virtualCursor[aPivotMoveMethod](aRule);
    SimpleTest.ok((aIdOrNameOrAcc && moved) || (!aIdOrNameOrAcc && !moved),
                  "moved pivot");
  };

  this.getID = function setVirtualCursorPosInvoker_getID()
  {
    return "Do " + (aIdOrNameOrAcc ? "" : "no-op ") + aPivotMoveMethod;
  }

  if (aIdOrNameOrAcc) {
    this.eventSeq = [ new virtualCursorChangedChecker(aDocAcc, aIdOrNameOrAcc) ];
  } else {
    this.eventSeq = [];
    this.unexpectedEventSeq = [
      new invokerChecker(EVENT_VIRTUALCURSOR_CHANGED, aDocAcc)
    ];
  }
}

/**
 * Add invokers to a queue to test a rule and an expected sequence of element ids
 * or accessible names for that rule in the given document.
 *
 * @param aQueue event queue in which to push invoker sequence.
 * @param aDocAcc the managing document of the virtual cursor we are testing
 * @param aRule the traversal rule to use in the invokers
 * @param aSequence a sequence of accessible names or elemnt ids to expect with
 *   the given rule in the given document
 */
function queueTraversalSequence(aQueue, aDocAcc, aRule, aSequence)
{
  aDocAcc.virtualCursor.position = null;

  for (var i = 0; i < aSequence.length; i++) {
    var invoker = new setVirtualCursorPosInvoker(aDocAcc, "moveNext",
                                                 aRule, aSequence[i]);
    aQueue.push(invoker);
  }

  // No further more matches for given rule, expect no virtual cursor changes.
  aQueue.push(new setVirtualCursorPosInvoker(aDocAcc, "moveNext", aRule, null));

  for (var i = aSequence.length-2; i >= 0; i--) {
    var invoker = new setVirtualCursorPosInvoker(aDocAcc, "movePrevious",
                                                 aRule, aSequence[i])
    aQueue.push(invoker);
  }

  // No previous more matches for given rule, expect no virtual cursor changes.
  aQueue.push(new setVirtualCursorPosInvoker(aDocAcc, "movePrevious", aRule, null));

  aQueue.push(new setVirtualCursorPosInvoker(
    aDocAcc, "moveLast", aRule, aSequence[aSequence.length - 1]));

  // No further more matches for given rule, expect no virtual cursor changes.
  aQueue.push(new setVirtualCursorPosInvoker(aDocAcc, "moveNext", aRule, null));

  aQueue.push(new setVirtualCursorPosInvoker(
    aDocAcc, "moveFirst", aRule, aSequence[0]));

  // No previous more matches for given rule, expect no virtual cursor changes.
  aQueue.push(new setVirtualCursorPosInvoker(aDocAcc, "movePrevious", aRule, null));
}

/**
 * A debug utility for writing proper sequences for queueTraversalSequence.
 */
function dumpTraversalSequence(aPivot, aRule)
{
  var sequence = []
  if (aPivot.moveFirst(aRule)) {
    do {
      sequence.push("'" + prettyName(aPivot.position) + "'");
    } while (aPivot.moveNext(aRule))
  }
  SimpleTest.info("\n[" + sequence.join(", ") + "]\n");
}
