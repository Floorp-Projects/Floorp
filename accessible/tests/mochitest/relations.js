////////////////////////////////////////////////////////////////////////////////
// Constants

const RELATION_CONTROLLED_BY = nsIAccessibleRelation.RELATION_CONTROLLED_BY;
const RELATION_CONTROLLER_FOR = nsIAccessibleRelation.RELATION_CONTROLLER_FOR;
const RELATION_DEFAULT_BUTTON = nsIAccessibleRelation.RELATION_DEFAULT_BUTTON;
const RELATION_DESCRIBED_BY = nsIAccessibleRelation.RELATION_DESCRIBED_BY;
const RELATION_DESCRIPTION_FOR = nsIAccessibleRelation.RELATION_DESCRIPTION_FOR;
const RELATION_EMBEDDED_BY = nsIAccessibleRelation.RELATION_EMBEDDED_BY;
const RELATION_EMBEDS = nsIAccessibleRelation.RELATION_EMBEDS;
const RELATION_FLOWS_FROM = nsIAccessibleRelation.RELATION_FLOWS_FROM;
const RELATION_FLOWS_TO = nsIAccessibleRelation.RELATION_FLOWS_TO;
const RELATION_LABEL_FOR = nsIAccessibleRelation.RELATION_LABEL_FOR;
const RELATION_LABELLED_BY = nsIAccessibleRelation.RELATION_LABELLED_BY;
const RELATION_MEMBER_OF = nsIAccessibleRelation.RELATION_MEMBER_OF;
const RELATION_NODE_CHILD_OF = nsIAccessibleRelation.RELATION_NODE_CHILD_OF;
const RELATION_PARENT_WINDOW_OF = nsIAccessibleRelation.RELATION_PARENT_WINDOW_OF;
const RELATION_POPUP_FOR = nsIAccessibleRelation.RELATION_POPUP_FOR;
const RELATION_SUBWINDOW_OF = nsIAccessibleRelation.RELATION_SUBWINDOW_OF;

////////////////////////////////////////////////////////////////////////////////
// General

/**
 * Test the accessible relation.
 *
 * @param aIdentifier        [in] identifier to get an accessible implementing
 *                           the given interfaces may be ID attribute or DOM
 *                           element or accessible object
 * @param aRelType           [in] relation type (see constants above)
 * @param aRelatedIdentifier [in] identifier of expected related accessible
 */
function testRelation(aIdentifier, aRelType, aRelatedIdentifier)
{
  var actualRelatedAcc = getRelatedAccessible(aIdentifier, aRelType);

  var relDescr = getRelationErrorMsg(aIdentifier, aRelType);
  if (!actualRelatedAcc && !aRelatedIdentifier) {
    ok(true, "No" + relDescr);
    return;
  }

  var relatedAcc = getAccessible(aRelatedIdentifier);
  if (!relatedAcc)
    return;

  is(actualRelatedAcc, relatedAcc,
      aRelatedIdentifier + " is not a target of" + relDescr);
}

/**
 * Return related accessible for the given relation type.
 *
 * @param aIdentifier  [in] identifier to get an accessible implementing
 *                     the given interfaces may be ID attribute or DOM
 *                     element or accessible object
 * @param aRelType     [in] relation type (see constants above)
 */
function getRelatedAccessible(aIdentifier, aRelType)
{
  var acc = getAccessible(aIdentifier);
  if (!acc)
    return;

  var relatedAcc = null;
  try {
    relatedAcc = acc.getAccessibleRelated(aRelType);
  } catch (e) {
    ok(false, "Can't get" + getRelationErrorMsg(aIdentifier, aRelType));
  }

  return relatedAcc;
}

////////////////////////////////////////////////////////////////////////////////
// Private implementation details

function getRelationErrorMsg(aIdentifier, aRelType)
{
  var relStr = relationTypeToString(aRelType);
  return " relation of '" + relStr + "' type for " + aIdentifier + ".";
}
