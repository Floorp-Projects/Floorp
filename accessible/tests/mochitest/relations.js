/* import-globals-from common.js */

// //////////////////////////////////////////////////////////////////////////////
// Constants

var RELATION_CONTROLLED_BY = nsIAccessibleRelation.RELATION_CONTROLLED_BY;
var RELATION_CONTROLLER_FOR = nsIAccessibleRelation.RELATION_CONTROLLER_FOR;
var RELATION_DEFAULT_BUTTON = nsIAccessibleRelation.RELATION_DEFAULT_BUTTON;
var RELATION_DESCRIBED_BY = nsIAccessibleRelation.RELATION_DESCRIBED_BY;
var RELATION_DESCRIPTION_FOR = nsIAccessibleRelation.RELATION_DESCRIPTION_FOR;
var RELATION_EMBEDDED_BY = nsIAccessibleRelation.RELATION_EMBEDDED_BY;
var RELATION_EMBEDS = nsIAccessibleRelation.RELATION_EMBEDS;
var RELATION_FLOWS_FROM = nsIAccessibleRelation.RELATION_FLOWS_FROM;
var RELATION_FLOWS_TO = nsIAccessibleRelation.RELATION_FLOWS_TO;
var RELATION_LABEL_FOR = nsIAccessibleRelation.RELATION_LABEL_FOR;
var RELATION_LABELLED_BY = nsIAccessibleRelation.RELATION_LABELLED_BY;
var RELATION_MEMBER_OF = nsIAccessibleRelation.RELATION_MEMBER_OF;
var RELATION_NODE_CHILD_OF = nsIAccessibleRelation.RELATION_NODE_CHILD_OF;
var RELATION_NODE_PARENT_OF = nsIAccessibleRelation.RELATION_NODE_PARENT_OF;
var RELATION_PARENT_WINDOW_OF = nsIAccessibleRelation.RELATION_PARENT_WINDOW_OF;
var RELATION_POPUP_FOR = nsIAccessibleRelation.RELATION_POPUP_FOR;
var RELATION_SUBWINDOW_OF = nsIAccessibleRelation.RELATION_SUBWINDOW_OF;
var RELATION_CONTAINING_DOCUMENT =
  nsIAccessibleRelation.RELATION_CONTAINING_DOCUMENT;
var RELATION_CONTAINING_TAB_PANE =
  nsIAccessibleRelation.RELATION_CONTAINING_TAB_PANE;
var RELATION_CONTAINING_APPLICATION =
  nsIAccessibleRelation.RELATION_CONTAINING_APPLICATION;
const RELATION_DETAILS = nsIAccessibleRelation.RELATION_DETAILS;
const RELATION_DETAILS_FOR = nsIAccessibleRelation.RELATION_DETAILS_FOR;
const RELATION_ERRORMSG = nsIAccessibleRelation.RELATION_ERRORMSG;
const RELATION_ERRORMSG_FOR = nsIAccessibleRelation.RELATION_ERRORMSG_FOR;
const RELATION_LINKS_TO = nsIAccessibleRelation.RELATION_LINKS_TO;

// //////////////////////////////////////////////////////////////////////////////
// General

/**
 * Test the accessible relation.
 *
 * @param aIdentifier          [in] identifier to get an accessible, may be ID
 *                             attribute or DOM element or accessible object
 * @param aRelType             [in] relation type (see constants above)
 * @param aRelatedIdentifiers  [in] identifier or array of identifiers of
 *                             expected related accessibles
 */
function testRelation(aIdentifier, aRelType, aRelatedIdentifiers) {
  var relation = getRelationByType(aIdentifier, aRelType);

  var relDescr = getRelationErrorMsg(aIdentifier, aRelType);
  var relDescrStart = getRelationErrorMsg(aIdentifier, aRelType, true);

  if (!relation || !relation.targetsCount) {
    if (!aRelatedIdentifiers) {
      ok(true, "No" + relDescr);
      return;
    }

    var msg =
      relDescrStart +
      "has no expected targets: '" +
      prettyName(aRelatedIdentifiers) +
      "'";

    ok(false, msg);
    return;
  } else if (!aRelatedIdentifiers) {
    ok(false, "There are unexpected targets of " + relDescr);
    return;
  }

  var relatedIds =
    aRelatedIdentifiers instanceof Array
      ? aRelatedIdentifiers
      : [aRelatedIdentifiers];

  var targets = [];
  for (let idx = 0; idx < relatedIds.length; idx++) {
    targets.push(getAccessible(relatedIds[idx]));
  }

  if (targets.length != relatedIds.length) {
    return;
  }

  var actualTargets = relation.getTargets();

  // Check if all given related accessibles are targets of obtained relation.
  for (let idx = 0; idx < targets.length; idx++) {
    var isFound = false;
    for (let relatedAcc of actualTargets.enumerate(Ci.nsIAccessible)) {
      if (targets[idx] == relatedAcc) {
        isFound = true;
        break;
      }
    }

    ok(isFound, prettyName(relatedIds[idx]) + " is not a target of" + relDescr);
  }

  // Check if all obtained targets are given related accessibles.
  for (let relatedAcc of actualTargets.enumerate(Ci.nsIAccessible)) {
    let idx;
    // eslint-disable-next-line no-empty
    for (idx = 0; idx < targets.length && relatedAcc != targets[idx]; idx++) {}

    if (idx == targets.length) {
      ok(
        false,
        "There is unexpected target" + prettyName(relatedAcc) + "of" + relDescr
      );
    }
  }
}

/**
 * Test that the given accessible relations don't exist.
 *
 * @param aIdentifier           [in] identifier to get an accessible, may be ID
 *                              attribute or DOM element or accessible object
 * @param aRelType              [in] relation type (see constants above)
 * @param aUnrelatedIdentifiers [in] identifier or array of identifiers of
 *                              accessibles that shouldn't exist for this
 *                              relation.
 */
function testAbsentRelation(aIdentifier, aRelType, aUnrelatedIdentifiers) {
  var relation = getRelationByType(aIdentifier, aRelType);

  var relDescr = getRelationErrorMsg(aIdentifier, aRelType);

  if (!aUnrelatedIdentifiers) {
    ok(false, "No identifiers given for unrelated accessibles.");
    return;
  }

  if (!relation || !relation.targetsCount) {
    ok(true, "No relations exist.");
    return;
  }

  var relatedIds =
    aUnrelatedIdentifiers instanceof Array
      ? aUnrelatedIdentifiers
      : [aUnrelatedIdentifiers];

  var targets = [];
  for (let idx = 0; idx < relatedIds.length; idx++) {
    targets.push(getAccessible(relatedIds[idx]));
  }

  if (targets.length != relatedIds.length) {
    return;
  }

  var actualTargets = relation.getTargets();

  // Any found targets that match given accessibles should be called out.
  for (let idx = 0; idx < targets.length; idx++) {
    var notFound = true;
    for (let relatedAcc of actualTargets.enumerate(Ci.nsIAccessible)) {
      if (targets[idx] == relatedAcc) {
        notFound = false;
        break;
      }
    }

    ok(notFound, prettyName(relatedIds[idx]) + " is a target of " + relDescr);
  }
}

/**
 * Return related accessible for the given relation type.
 *
 * @param aIdentifier  [in] identifier to get an accessible, may be ID attribute
 *                     or DOM element or accessible object
 * @param aRelType     [in] relation type (see constants above)
 */
function getRelationByType(aIdentifier, aRelType) {
  var acc = getAccessible(aIdentifier);
  if (!acc) {
    return null;
  }

  var relation = null;
  try {
    relation = acc.getRelationByType(aRelType);
  } catch (e) {
    ok(false, "Can't get" + getRelationErrorMsg(aIdentifier, aRelType));
  }

  return relation;
}

// //////////////////////////////////////////////////////////////////////////////
// Private implementation details

function getRelationErrorMsg(aIdentifier, aRelType, aIsStartSentence) {
  var relStr = relationTypeToString(aRelType);
  var msg = aIsStartSentence ? "Relation of '" : " relation of '";
  msg += relStr + "' type for '" + prettyName(aIdentifier) + "'";
  msg += aIsStartSentence ? " " : ".";

  return msg;
}
