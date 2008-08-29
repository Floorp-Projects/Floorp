////////////////////////////////////////////////////////////////////////////////
// Interfaces

const nsIAccessibleRetrieval = Components.interfaces.nsIAccessibleRetrieval;

const nsIAccessibleEvent = Components.interfaces.nsIAccessibleEvent;
const nsIAccessibleStates = Components.interfaces.nsIAccessibleStates;
const nsIAccessibleRole = Components.interfaces.nsIAccessibleRole;
const nsIAccessibleTypes = Components.interfaces.nsIAccessibleTypes;

const nsIAccessibleRelation = Components.interfaces.nsIAccessibleRelation;

const nsIAccessNode = Components.interfaces.nsIAccessNode;
const nsIAccessible = Components.interfaces.nsIAccessible;

const nsIAccessibleDocument = Components.interfaces.nsIAccessibleDocument;

const nsIAccessibleText = Components.interfaces.nsIAccessibleText;
const nsIAccessibleEditableText = Components.interfaces.nsIAccessibleEditableText;

const nsIAccessibleHyperLink = Components.interfaces.nsIAccessibleHyperLink;
const nsIAccessibleHyperText = Components.interfaces.nsIAccessibleHyperText;

const nsIAccessibleImage = Components.interfaces.nsIAccessibleImage;
const nsIAccessibleSelectable = Components.interfaces.nsIAccessibleSelectable;
const nsIAccessibleTable = Components.interfaces.nsIAccessibleTable;
const nsIAccessibleValue = Components.interfaces.nsIAccessibleValue;

const nsIObserverService = Components.interfaces.nsIObserverService;

const nsIDOMNode = Components.interfaces.nsIDOMNode;

////////////////////////////////////////////////////////////////////////////////
// General

/**
 * nsIAccessibleRetrieval, initialized when test is loaded.
 */
var gAccRetrieval = null;

/**
 * Return accessible for the given ID attribute or DOM element.
 *
 * @param aElmOrID     [in] the ID attribute or DOM element to get an accessible
 *                     for
 * @param aInterfaces  [in, optional] the accessible interface or the array of
 *                     accessible interfaces to query it/them from obtained
 *                     accessible
 * @param aElmObj      [out, optional] object to store DOM element which
 *                     accessible is created for
 */
function getAccessible(aElmOrID, aInterfaces, aElmObj)
{
  var elm = null;

  if (aElmOrID instanceof nsIDOMNode) {
    elm = aElmOrID;
  } else {
    var elm = document.getElementById(aElmOrID);
    if (!elm) {
      ok(false, "Can't get DOM element for " + aID);
      return null;
    }
  }

  if (aElmObj && (typeof aElmObj == "object"))
    aElmObj.value = elm;

  var acc = null;
  try {
    acc = gAccRetrieval.getAccessibleFor(elm);
  } catch (e) {
  }
  
  if (!acc) {
    ok(false, "Can't get accessible for " + aID);
    return null;
  }
  
  if (!aInterfaces)
    return acc;
  
  if (aInterfaces instanceof Array) {
    for (var index = 0; index < aInterfaces.length; index++) {
      try {
        acc.QueryInterface(aInterfaces[index]);
      } catch (e) {
        ok(false, "Can't query " + aInterfaces[index] + " for " + aID);
        return null;
      }
    }
    return acc;
  }
  
  try {
    acc.QueryInterface(aInterfaces);
  } catch (e) {
    ok(false, "Can't query " + aInterfaces + " for " + aID);
    return null;
  }
  
  return acc;
}

////////////////////////////////////////////////////////////////////////////////
// Private
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// General

function initialize()
{
  gAccRetrieval = Components.classes["@mozilla.org/accessibleRetrieval;1"].
  getService(nsIAccessibleRetrieval);
}

addLoadEvent(initialize);
