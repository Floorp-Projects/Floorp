Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

////////////////////////////////////////////////////////////////////////////////
// Constants

const PREFILTER_INVISIBLE = nsIAccessibleTraversalRule.PREFILTER_INVISIBLE;
const FILTER_MATCH = nsIAccessibleTraversalRule.FILTER_MATCH;
const FILTER_IGNORE = nsIAccessibleTraversalRule.FILTER_IGNORE;
const FILTER_IGNORE_SUBTREE = nsIAccessibleTraversalRule.FILTER_IGNORE_SUBTREE;

const NS_ERROR_NOT_IN_TREE = 0x80780026;

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
function VCChangedChecker(aDocAcc, aIdOrNameOrAcc, aTextOffsets)
{
  this.__proto__ = new invokerChecker(EVENT_VIRTUALCURSOR_CHANGED, aDocAcc);

  this.check = function VCChangedChecker_check(aEvent)
  {
    SimpleTest.info("VCChangedChecker_check");

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

    var prevPosAndOffset = VCChangedChecker.
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

VCChangedChecker.prevPosAndOffset = {};

VCChangedChecker.storePreviousPosAndOffset =
  function storePreviousPosAndOffset(aPivot)
{
  VCChangedChecker.prevPosAndOffset[aPivot] =
    {position: aPivot.position,
     startOffset: aPivot.startOffset,
     endOffset: aPivot.endOffset};
};

VCChangedChecker.getPreviousPosAndOffset =
  function getPreviousPosAndOffset(aPivot)
{
  return VCChangedChecker.prevPosAndOffset[aPivot];
};

/**
 * Set a text range in the pivot and wait for virtual cursor change event.
 *
 * @param aDocAcc         [in] document that manages the virtual cursor
 * @param aTextAccessible [in] accessible to set to virtual cursor's position
 * @param aTextOffsets    [in] start and end offsets of text range to set in
 *                        virtual cursor.
 */
function setVCRangeInvoker(aDocAcc, aTextAccessible, aTextOffsets)
{
  this.invoke = function virtualCursorChangedInvoker_invoke()
  {
    VCChangedChecker.
      storePreviousPosAndOffset(aDocAcc.virtualCursor);
    SimpleTest.info(prettyName(aTextAccessible) + " " + aTextOffsets);
    aDocAcc.virtualCursor.setTextRange(aTextAccessible,
                                       aTextOffsets[0],
                                       aTextOffsets[1]);
  };

  this.getID = function setVCRangeInvoker_getID()
  {
    return "Set offset in " + prettyName(aTextAccessible) +
      " to (" + aTextOffsets[0] + ", " + aTextOffsets[1] + ")";
  };

  this.eventSeq = [
    new VCChangedChecker(aDocAcc, aTextAccessible, aTextOffsets)
  ];
}

/**
 * Move the pivot and wait for virtual cursor change event.
 *
 * @param aDocAcc          [in] document that manages the virtual cursor
 * @param aPivotMoveMethod [in] method to test (ie. "moveNext", "moveFirst", etc.)
 * @param aRule            [in] traversal rule object
 * @param aIdOrNameOrAcc   [in] id, accessivle or accessible name to expect
 *                         virtual cursor to land on after performing move method.
 */
function setVCPosInvoker(aDocAcc, aPivotMoveMethod, aRule, aIdOrNameOrAcc)
{
  this.invoke = function virtualCursorChangedInvoker_invoke()
  {
    VCChangedChecker.
      storePreviousPosAndOffset(aDocAcc.virtualCursor);
    var moved = aDocAcc.virtualCursor[aPivotMoveMethod](aRule);
    SimpleTest.ok((aIdOrNameOrAcc && moved) || (!aIdOrNameOrAcc && !moved),
                  "moved pivot");
  };

  this.getID = function setVCPosInvoker_getID()
  {
    return "Do " + (aIdOrNameOrAcc ? "" : "no-op ") + aPivotMoveMethod;
  };

  if (aIdOrNameOrAcc) {
    this.eventSeq = [ new VCChangedChecker(aDocAcc, aIdOrNameOrAcc) ];
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
 * @param aQueue    [in] event queue in which to push invoker sequence.
 * @param aDocAcc   [in] the managing document of the virtual cursor we are testing
 * @param aRule     [in] the traversal rule to use in the invokers
 * @param aSequence [in] a sequence of accessible names or elemnt ids to expect with
 *                  the given rule in the given document
 */
function queueTraversalSequence(aQueue, aDocAcc, aRule, aSequence)
{
  aDocAcc.virtualCursor.position = null;

  for (var i = 0; i < aSequence.length; i++) {
    var invoker =
      new setVCPosInvoker(aDocAcc, "moveNext", aRule, aSequence[i]);
    aQueue.push(invoker);
  }

  // No further more matches for given rule, expect no virtual cursor changes.
  aQueue.push(new setVCPosInvoker(aDocAcc, "moveNext", aRule, null));

  for (var i = aSequence.length-2; i >= 0; i--) {
    var invoker =
      new setVCPosInvoker(aDocAcc, "movePrevious", aRule, aSequence[i]);
    aQueue.push(invoker);
  }

  // No previous more matches for given rule, expect no virtual cursor changes.
  aQueue.push(new setVCPosInvoker(aDocAcc, "movePrevious", aRule, null));

  aQueue.push(new setVCPosInvoker(aDocAcc, "moveLast", aRule,
                                  aSequence[aSequence.length - 1]));

  // No further more matches for given rule, expect no virtual cursor changes.
  aQueue.push(new setVCPosInvoker(aDocAcc, "moveNext", aRule, null));

  aQueue.push(new setVCPosInvoker(aDocAcc, "moveFirst", aRule, aSequence[0]));

  // No previous more matches for given rule, expect no virtual cursor changes.
  aQueue.push(new setVCPosInvoker(aDocAcc, "movePrevious", aRule, null));
}

/**
 * A checker for removing an accessible while the virtual cursor is on it.
 */
function removeVCPositionChecker(aDocAcc, aHiddenParentAcc)
{
  this.__proto__ = new invokerChecker(EVENT_REORDER, aHiddenParentAcc);

  this.check = function removeVCPositionChecker_check(aEvent) {
    var errorResult = 0;
    try {
      aDocAcc.virtualCursor.moveNext(ObjectTraversalRule);
    } catch (x) {
      errorResult = x.result;
    }
    SimpleTest.is(
      errorResult, NS_ERROR_NOT_IN_TREE,
      "Expecting NOT_IN_TREE error when moving pivot from invalid position.");
  };
}

/**
 * Put the virtual cursor's position on an object, and then remove it.
 *
 * @param aDocAcc     [in] document that manages the virtual cursor
 * @param aPosNode    [in] DOM node to hide after virtual cursor's position is
 *                    set to it.
 */
function removeVCPositionInvoker(aDocAcc, aPosNode)
{
  this.accessible = getAccessible(aPosNode);
  this.invoke = function removeVCPositionInvoker_invoke()
  {
    aDocAcc.virtualCursor.position = this.accessible;
    aPosNode.parentNode.removeChild(aPosNode);
  };

  this.getID = function removeVCPositionInvoker_getID()
  {
    return "Bring virtual cursor to accessible, and remove its DOM node.";
  };

  this.eventSeq = [
    new removeVCPositionChecker(aDocAcc, this.accessible.parent)
  ];
}

/**
 * A checker for removing the pivot root and then calling moveFirst, and
 * checking that an exception is thrown.
 */
function removeVCRootChecker(aPivot)
{
  this.__proto__ = new invokerChecker(EVENT_REORDER, aPivot.root.parent);

  this.check = function removeVCRootChecker_check(aEvent) {
    var errorResult = 0;
    try {
      aPivot.moveLast(ObjectTraversalRule);
    } catch (x) {
      errorResult = x.result;
    }
    SimpleTest.is(
      errorResult, NS_ERROR_NOT_IN_TREE,
      "Expecting NOT_IN_TREE error when moving pivot from invalid position.");
  };
}

/**
 * Create a pivot, remove its root, and perform an operation where the root is
 * needed.
 *
 * @param aRootNode [in] DOM node of which accessible will be the root of the
 *                       pivot. Should have more than one child.
 */
function removeVCRootInvoker(aRootNode)
{
  this.pivot = gAccRetrieval.createAccessiblePivot(getAccessible(aRootNode));
  this.invoke = function removeVCRootInvoker_invoke()
  {
    this.pivot.position = this.pivot.root.firstChild;
    aRootNode.parentNode.removeChild(aRootNode);
  };

  this.getID = function removeVCRootInvoker_getID()
  {
    return "Remove root of pivot from tree.";
  };

  this.eventSeq = [
    new removeVCRootChecker(this.pivot)
  ];
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
