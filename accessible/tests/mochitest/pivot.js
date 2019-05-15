ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

// //////////////////////////////////////////////////////////////////////////////
// Constants

const PREFILTER_INVISIBLE = nsIAccessibleTraversalRule.PREFILTER_INVISIBLE;
const PREFILTER_TRANSPARENT = nsIAccessibleTraversalRule.PREFILTER_TRANSPARENT;
const FILTER_MATCH = nsIAccessibleTraversalRule.FILTER_MATCH;
const FILTER_IGNORE = nsIAccessibleTraversalRule.FILTER_IGNORE;
const FILTER_IGNORE_SUBTREE = nsIAccessibleTraversalRule.FILTER_IGNORE_SUBTREE;
const NO_BOUNDARY = nsIAccessiblePivot.NO_BOUNDARY;
const CHAR_BOUNDARY = nsIAccessiblePivot.CHAR_BOUNDARY;
const WORD_BOUNDARY = nsIAccessiblePivot.WORD_BOUNDARY;

const NS_ERROR_NOT_IN_TREE = 0x80780026;
const NS_ERROR_INVALID_ARG = 0x80070057;

// //////////////////////////////////////////////////////////////////////////////
// Traversal rules

/**
 * Rule object to traverse all focusable nodes and text nodes.
 */
var HeadersTraversalRule =
{
  getMatchRoles() {
    return [ROLE_HEADING];
  },

  preFilter: PREFILTER_INVISIBLE,

  match(aAccessible) {
    return FILTER_MATCH;
  },

  QueryInterface: ChromeUtils.generateQI([nsIAccessibleTraversalRule]),
};

/**
 * Traversal rule for all focusable nodes or leafs.
 */
var ObjectTraversalRule =
{
  getMatchRoles() {
    return [];
  },

  preFilter: PREFILTER_INVISIBLE | PREFILTER_TRANSPARENT,

  match(aAccessible) {
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

  QueryInterface: ChromeUtils.generateQI([nsIAccessibleTraversalRule]),
};

// //////////////////////////////////////////////////////////////////////////////
// Virtual state invokers and checkers

/**
 * A checker for virtual cursor changed events.
 */
function VCChangedChecker(aDocAcc, aIdOrNameOrAcc, aTextOffsets, aPivotMoveMethod,
                          aIsFromUserInput, aBoundaryType = NO_BOUNDARY) {
  this.__proto__ = new invokerChecker(EVENT_VIRTUALCURSOR_CHANGED, aDocAcc);

  this.match = function VCChangedChecker_match(aEvent) {
    var event = null;
    try {
      event = aEvent.QueryInterface(nsIAccessibleVirtualCursorChangeEvent);
    } catch (e) {
      return false;
    }

    var expectedReason = VCChangedChecker.methodReasonMap[aPivotMoveMethod] ||
      nsIAccessiblePivot.REASON_NONE;

    return event.reason == expectedReason &&
           event.boundaryType == aBoundaryType;
  };

  this.check = function VCChangedChecker_check(aEvent) {
    SimpleTest.info("VCChangedChecker_check");

    var event = null;
    try {
      event = aEvent.QueryInterface(nsIAccessibleVirtualCursorChangeEvent);
    } catch (e) {
      SimpleTest.ok(false, "Does not support correct interface: " + e);
    }

    var position = aDocAcc.virtualCursor.position;
    var idMatches = position && position.DOMNode.id == aIdOrNameOrAcc;
    var nameMatches = position && position.name == aIdOrNameOrAcc;
    var accMatches = position == aIdOrNameOrAcc;

    SimpleTest.ok(idMatches || nameMatches || accMatches,
                  "id or name matches - expecting " +
                  prettyName(aIdOrNameOrAcc) + ", got '" + prettyName(position));

    SimpleTest.is(aEvent.isFromUserInput, aIsFromUserInput,
                  "Expected user input is " + aIsFromUserInput + "\n");

    SimpleTest.is(event.newAccessible, position,
                  "new position in event is incorrect");

    if (aTextOffsets) {
      SimpleTest.is(aDocAcc.virtualCursor.startOffset, aTextOffsets[0],
                    "wrong start offset");
      SimpleTest.is(aDocAcc.virtualCursor.endOffset, aTextOffsets[1],
                    "wrong end offset");
      SimpleTest.is(event.newStartOffset, aTextOffsets[0],
                    "wrong start offset in event");
      SimpleTest.is(event.newEndOffset, aTextOffsets[1],
                    "wrong end offset in event");
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
  function storePreviousPosAndOffset(aPivot) {
  VCChangedChecker.prevPosAndOffset[aPivot] =
    {position: aPivot.position,
     startOffset: aPivot.startOffset,
     endOffset: aPivot.endOffset};
};

VCChangedChecker.getPreviousPosAndOffset =
  function getPreviousPosAndOffset(aPivot) {
  return VCChangedChecker.prevPosAndOffset[aPivot];
};

VCChangedChecker.methodReasonMap = {
  "moveNext": nsIAccessiblePivot.REASON_NEXT,
  "movePrevious": nsIAccessiblePivot.REASON_PREV,
  "moveFirst": nsIAccessiblePivot.REASON_FIRST,
  "moveLast": nsIAccessiblePivot.REASON_LAST,
  "setTextRange": nsIAccessiblePivot.REASON_NONE,
  "moveNextByText": nsIAccessiblePivot.REASON_NEXT,
  "movePreviousByText": nsIAccessiblePivot.REASON_PREV,
  "moveToPoint": nsIAccessiblePivot.REASON_POINT,
};

/**
 * Set a text range in the pivot and wait for virtual cursor change event.
 *
 * @param aDocAcc         [in] document that manages the virtual cursor
 * @param aTextAccessible [in] accessible to set to virtual cursor's position
 * @param aTextOffsets    [in] start and end offsets of text range to set in
 *                        virtual cursor.
 */
function setVCRangeInvoker(aDocAcc, aTextAccessible, aTextOffsets) {
  this.invoke = function virtualCursorChangedInvoker_invoke() {
    VCChangedChecker.
      storePreviousPosAndOffset(aDocAcc.virtualCursor);
    SimpleTest.info(prettyName(aTextAccessible) + " " + aTextOffsets);
    aDocAcc.virtualCursor.setTextRange(aTextAccessible,
                                       aTextOffsets[0],
                                       aTextOffsets[1]);
  };

  this.getID = function setVCRangeInvoker_getID() {
    return "Set offset in " + prettyName(aTextAccessible) +
      " to (" + aTextOffsets[0] + ", " + aTextOffsets[1] + ")";
  };

  this.eventSeq = [
    new VCChangedChecker(aDocAcc, aTextAccessible, aTextOffsets, "setTextRange", true),
  ];
}

/**
 * Move the pivot and wait for virtual cursor change event.
 *
 * @param aDocAcc          [in] document that manages the virtual cursor
 * @param aPivotMoveMethod [in] method to test (ie. "moveNext", "moveFirst", etc.)
 * @param aRule            [in] traversal rule object
 * @param aIdOrNameOrAcc   [in] id, accessible or accessible name to expect
 *                         virtual cursor to land on after performing move method.
 *                         false if no move is expected.
 * @param aIsFromUserInput [in] set user input flag when invoking method, and
 *                         expect it in the event.
 */
function setVCPosInvoker(aDocAcc, aPivotMoveMethod, aRule, aIdOrNameOrAcc,
                         aIsFromUserInput) {
  // eslint-disable-next-line mozilla/no-compare-against-boolean-literals
  var expectMove = aIdOrNameOrAcc != false;
  this.invoke = function virtualCursorChangedInvoker_invoke() {
    VCChangedChecker.
      storePreviousPosAndOffset(aDocAcc.virtualCursor);
    if (aPivotMoveMethod && aRule) {
      var moved = false;
      switch (aPivotMoveMethod) {
        case "moveFirst":
        case "moveLast":
          moved = aDocAcc.virtualCursor[aPivotMoveMethod](aRule,
            aIsFromUserInput === undefined ? true : aIsFromUserInput);
          break;
        case "moveNext":
        case "movePrevious":
          moved = aDocAcc.virtualCursor[aPivotMoveMethod](aRule,
            aDocAcc.virtualCursor.position, false,
            aIsFromUserInput === undefined ? true : aIsFromUserInput);
          break;
      }
      SimpleTest.is(!!moved, !!expectMove,
                    "moved pivot with " + aPivotMoveMethod +
                    " to " + aIdOrNameOrAcc);
    } else {
      aDocAcc.virtualCursor.position = getAccessible(aIdOrNameOrAcc);
    }
  };

  this.getID = function setVCPosInvoker_getID() {
    return "Do " + (expectMove ? "" : "no-op ") + aPivotMoveMethod;
  };

  if (expectMove) {
    this.eventSeq = [
      new VCChangedChecker(aDocAcc, aIdOrNameOrAcc, null, aPivotMoveMethod,
        aIsFromUserInput === undefined ? !!aPivotMoveMethod : aIsFromUserInput),
    ];
  } else {
    this.eventSeq = [];
    this.unexpectedEventSeq = [
      new invokerChecker(EVENT_VIRTUALCURSOR_CHANGED, aDocAcc),
    ];
  }
}

/**
 * Move the pivot by text and wait for virtual cursor change event.
 *
 * @param aDocAcc          [in] document that manages the virtual cursor
 * @param aPivotMoveMethod [in] method to test (ie. "moveNext", "moveFirst", etc.)
 * @param aBoundary        [in] boundary constant
 * @param aTextOffsets     [in] start and end offsets of text range to set in
 *                         virtual cursor.
 * @param aIdOrNameOrAcc   [in] id, accessible or accessible name to expect
 *                         virtual cursor to land on after performing move method.
 *                         false if no move is expected.
 * @param aIsFromUserInput [in] set user input flag when invoking method, and
 *                         expect it in the event.
 */
function setVCTextInvoker(aDocAcc, aPivotMoveMethod, aBoundary, aTextOffsets,
                          aIdOrNameOrAcc, aIsFromUserInput) {
  // eslint-disable-next-line mozilla/no-compare-against-boolean-literals
  var expectMove = aIdOrNameOrAcc != false;
  this.invoke = function virtualCursorChangedInvoker_invoke() {
    VCChangedChecker.storePreviousPosAndOffset(aDocAcc.virtualCursor);
    SimpleTest.info(aDocAcc.virtualCursor.position);
    var moved = aDocAcc.virtualCursor[aPivotMoveMethod](aBoundary,
      aIsFromUserInput === undefined);
    SimpleTest.is(!!moved, !!expectMove,
                  "moved pivot by text with " + aPivotMoveMethod +
                  " to " + aIdOrNameOrAcc);
  };

  this.getID = function setVCPosInvoker_getID() {
    return "Do " + (expectMove ? "" : "no-op ") + aPivotMoveMethod + " in " +
      prettyName(aIdOrNameOrAcc) + ", " + boundaryToString(aBoundary) +
      ", [" + aTextOffsets + "]";
  };

  if (expectMove) {
    this.eventSeq = [
      new VCChangedChecker(aDocAcc, aIdOrNameOrAcc, aTextOffsets, aPivotMoveMethod,
        aIsFromUserInput === undefined ? true : aIsFromUserInput, aBoundary),
    ];
  } else {
    this.eventSeq = [];
    this.unexpectedEventSeq = [
      new invokerChecker(EVENT_VIRTUALCURSOR_CHANGED, aDocAcc),
    ];
  }
}


/**
 * Move the pivot to the position under the point.
 *
 * @param aDocAcc        [in] document that manages the virtual cursor
 * @param aX             [in] screen x coordinate
 * @param aY             [in] screen y coordinate
 * @param aIgnoreNoMatch [in] don't unset position if no object was found at
 *                       point.
 * @param aRule          [in] traversal rule object
 * @param aIdOrNameOrAcc [in] id, accessible or accessible name to expect
 *                       virtual cursor to land on after performing move method.
 *                       false if no move is expected.
 */
function moveVCCoordInvoker(aDocAcc, aX, aY, aIgnoreNoMatch,
                            aRule, aIdOrNameOrAcc) {
  // eslint-disable-next-line mozilla/no-compare-against-boolean-literals
  var expectMove = aIdOrNameOrAcc != false;
  this.invoke = function virtualCursorChangedInvoker_invoke() {
    VCChangedChecker.
      storePreviousPosAndOffset(aDocAcc.virtualCursor);
    var moved = aDocAcc.virtualCursor.moveToPoint(aRule, aX, aY,
                                                  aIgnoreNoMatch);
    SimpleTest.ok((expectMove && moved) || (!expectMove && !moved),
                  "moved pivot");
  };

  this.getID = function setVCPosInvoker_getID() {
    return "Do " + (expectMove ? "" : "no-op ") + "moveToPoint " + aIdOrNameOrAcc;
  };

  if (expectMove) {
    this.eventSeq = [
      new VCChangedChecker(aDocAcc, aIdOrNameOrAcc, null, "moveToPoint", true),
    ];
  } else {
    this.eventSeq = [];
    this.unexpectedEventSeq = [
      new invokerChecker(EVENT_VIRTUALCURSOR_CHANGED, aDocAcc),
    ];
  }
}

/**
 * Change the pivot modalRoot
 *
 * @param aDocAcc         [in] document that manages the virtual cursor
 * @param aModalRootAcc   [in] accessible of the modal root, or null
 * @param aExpectedResult [in] error result expected. 0 if expecting success
 */
function setModalRootInvoker(aDocAcc, aModalRootAcc, aExpectedResult) {
  this.invoke = function setModalRootInvoker_invoke() {
    var errorResult = 0;
    try {
      aDocAcc.virtualCursor.modalRoot = aModalRootAcc;
    } catch (x) {
      SimpleTest.ok(
        x.result, "Unexpected exception when changing modal root: " + x);
      errorResult = x.result;
    }

    SimpleTest.is(errorResult, aExpectedResult,
                  "Did not get expected result when changing modalRoot");
  };

  this.getID = function setModalRootInvoker_getID() {
    return "Set modalRoot to " + prettyName(aModalRootAcc);
  };

  this.eventSeq = [];
  this.unexpectedEventSeq = [
    new invokerChecker(EVENT_VIRTUALCURSOR_CHANGED, aDocAcc),
  ];
}

/**
 * Add invokers to a queue to test a rule and an expected sequence of element ids
 * or accessible names for that rule in the given document.
 *
 * @param aQueue     [in] event queue in which to push invoker sequence.
 * @param aDocAcc    [in] the managing document of the virtual cursor we are
 *                   testing
 * @param aRule      [in] the traversal rule to use in the invokers
 * @param aModalRoot [in] a modal root to use in this traversal sequence
 * @param aSequence  [in] a sequence of accessible names or element ids to expect
 *                   with the given rule in the given document
 */
function queueTraversalSequence(aQueue, aDocAcc, aRule, aModalRoot, aSequence) {
  aDocAcc.virtualCursor.position = null;

  // Add modal root (if any)
  aQueue.push(new setModalRootInvoker(aDocAcc, aModalRoot, 0));

  aQueue.push(new setVCPosInvoker(aDocAcc, "moveFirst", aRule, aSequence[0]));

  for (let i = 1; i < aSequence.length; i++) {
    let invoker =
      new setVCPosInvoker(aDocAcc, "moveNext", aRule, aSequence[i]);
    aQueue.push(invoker);
  }

  // No further more matches for given rule, expect no virtual cursor changes.
  aQueue.push(new setVCPosInvoker(aDocAcc, "moveNext", aRule, false));

  for (let i = aSequence.length - 2; i >= 0; i--) {
    let invoker =
      new setVCPosInvoker(aDocAcc, "movePrevious", aRule, aSequence[i]);
    aQueue.push(invoker);
  }

  // No previous more matches for given rule, expect no virtual cursor changes.
  aQueue.push(new setVCPosInvoker(aDocAcc, "movePrevious", aRule, false));

  aQueue.push(new setVCPosInvoker(aDocAcc, "moveLast", aRule,
                                  aSequence[aSequence.length - 1]));

  // No further more matches for given rule, expect no virtual cursor changes.
  aQueue.push(new setVCPosInvoker(aDocAcc, "moveNext", aRule, false));

  // set isFromUserInput to false, just to test..
  aQueue.push(new setVCPosInvoker(aDocAcc, "moveFirst", aRule, aSequence[0], false));

  // No previous more matches for given rule, expect no virtual cursor changes.
  aQueue.push(new setVCPosInvoker(aDocAcc, "movePrevious", aRule, false));

  // Remove modal root (if any).
  aQueue.push(new setModalRootInvoker(aDocAcc, null, 0));
}

/**
 * A checker for removing an accessible while the virtual cursor is on it.
 */
function removeVCPositionChecker(aDocAcc, aHiddenParentAcc) {
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
function removeVCPositionInvoker(aDocAcc, aPosNode) {
  this.accessible = getAccessible(aPosNode);
  this.invoke = function removeVCPositionInvoker_invoke() {
    aDocAcc.virtualCursor.position = this.accessible;
    aPosNode.remove();
  };

  this.getID = function removeVCPositionInvoker_getID() {
    return "Bring virtual cursor to accessible, and remove its DOM node.";
  };

  this.eventSeq = [
    new removeVCPositionChecker(aDocAcc, this.accessible.parent),
  ];
}

/**
 * A checker for removing the pivot root and then calling moveFirst, and
 * checking that an exception is thrown.
 */
function removeVCRootChecker(aPivot) {
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
function removeVCRootInvoker(aRootNode) {
  this.pivot = gAccService.createAccessiblePivot(getAccessible(aRootNode));
  this.invoke = function removeVCRootInvoker_invoke() {
    this.pivot.position = this.pivot.root.firstChild;
    aRootNode.remove();
  };

  this.getID = function removeVCRootInvoker_getID() {
    return "Remove root of pivot from tree.";
  };

  this.eventSeq = [
    new removeVCRootChecker(this.pivot),
  ];
}

/**
 * A debug utility for writing proper sequences for queueTraversalSequence.
 */
function dumpTraversalSequence(aPivot, aRule) {
  var sequence = [];
  if (aPivot.moveFirst(aRule)) {
    do {
      sequence.push("'" + prettyName(aPivot.position) + "'");
    } while (aPivot.moveNext(aRule));
  }
  SimpleTest.info("\n[" + sequence.join(", ") + "]\n");
}
